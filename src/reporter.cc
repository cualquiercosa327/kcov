#include <reporter.hh>
#include <elf.hh>
#include <collector.hh>
#include <utils.hh>

#include <string>
#include <list>
#include <unordered_map>

using namespace kcov;

class Reporter : public IReporter, public IElf::IListener, public ICollector::IListener
{
public:
	Reporter(IElf &elf, ICollector &collector) :
		m_elf(elf), m_collector(collector),
		m_nrLines(0), m_nrExecutedLines(0)
	{
		m_elf.registerListener(*this);
		m_collector.registerListener(*this);
	}

	bool lineIsCode(const char *file, unsigned int lineNr)
	{
		return m_lines.find(LineId(file, lineNr)) != m_lines.end();
	}

	LineExecutionCount getLineExecutionCount(const char *file, unsigned int lineNr)
	{
		LineMap_t::iterator it = m_lines.find(LineId(file, lineNr));

		if (it == m_lines.end())
			return LineExecutionCount(0, 0);

		Line *line = it->second;

		return LineExecutionCount(line->hits(),
				line->possibleHits());
	}

	ExecutionSummary getExecutionSummary()
	{
		return ExecutionSummary(m_nrLines, m_nrExecutedLines);
	}

	void *marshal(size_t *szOut)
	{
		size_t sz = getMarshalSize();
		void *start;
		uint8_t *p;

		start = malloc(sz);
		if (!start)
			return NULL;
		p = marshalHeader((uint8_t *)start);

		for (LineMap_t::iterator it = m_lines.begin();
				it != m_lines.end();
				it++) {
			Line *cur = it->second;

			p = cur->marshal(p);
		}

		*szOut = sz;

		return start;
	}

	bool unMarshal(void *data, size_t sz)
	{
		uint8_t *start = (uint8_t *)data;
		uint8_t *p = start;
		size_t n;

		p = unMarshalHeader(p);
		n = (sz - (p - start)) / getMarshalEntrySize();

		// Should already be 0, but anyway
		for (AddrToLineMap_t::iterator it = m_addrToLine.begin();
				it != m_addrToLine.end();
				it++)
			it->second->clearHits();
		m_nrExecutedLines = 0;

		for (size_t i = 0; i < n; i++) {
			unsigned long addr;
			unsigned int hits;

			p = Line::unMarshal(p, &addr, &hits);
			Line *line = m_addrToLine[addr];

			if (!line)
				continue;

			if (!hits)
				continue;

			// Really an internal error, but let's not hang on corrupted data
			if (hits > line->possibleHits())
				hits = line->possibleHits();

			m_nrExecutedLines++;
			// Register all hits for this address
			while (hits--)
				line->registerHit(addr);
		}

		return true;
	}

private:
	size_t getMarshalEntrySize()
	{
		return 2 * sizeof(uint64_t);
	}

	size_t getMarshalSize()
	{
		size_t out = 0;

		for (LineMap_t::iterator it = m_lines.begin();
				it != m_lines.end();
				it++) {
			Line *cur = it->second;

			out += cur->m_addrs.size();
		}


		return out * getMarshalEntrySize();
	}

	uint8_t *marshalHeader(uint8_t *p)
	{
		return p;
	}

	uint8_t *unMarshalHeader(uint8_t *p)
	{
		return p;
	}

	/* Called when the ELF is parsed */
	void onLine(const char *file, unsigned int lineNr, unsigned long addr)
	{
		LineId key(file, lineNr);

		LineMap_t::iterator it = m_lines.find(key);
		Line *line;

		if (it == m_lines.end()) {
			if (!file_exists(file))
				return;

			line = new Line(key);
			m_lines[key] = line;
			m_nrLines++;
		} else {
			line = it->second;
		}

		line->addAddress(addr);
		m_addrToLine[addr] = line;
	}

	/* Called during runtime */
	void onBreakpoint(unsigned long addr)
	{
		AddrToLineMap_t::iterator it = m_addrToLine.find(addr);

		if (it == m_addrToLine.end())
			return;
		Line *line = it->second;

		m_nrExecutedLines += line->registerHit(addr);
	}


	class LineId
	{
	public:
		LineId(const char *fileName, int nr) :
			m_file(fileName), m_lineNr(nr)
		{
		}

		bool operator==(const LineId &other) const
		{
			return (m_file == other.m_file) && (m_lineNr == other.m_lineNr);
		}

		std::string m_file;
		unsigned int m_lineNr;
	};

	class LineIdHash
	{
	public:
		size_t operator()(const LineId &obj) const
		{
			return std::hash<std::string>()(obj.m_file) ^ std::hash<int>()(obj.m_lineNr);
		}
	};

	class Line
	{
	public:
		typedef std::unordered_map<unsigned long, int> AddrToHitsMap_t;

		Line(LineId id) : m_file(id.m_file),
				m_lineNr(id.m_lineNr)

		{
		}

		void addAddress(unsigned long addr)
		{
			m_addrs[addr] = 0;
		}

		unsigned int registerHit(unsigned long addr)
		{
			unsigned int out = !m_addrs[addr];

			m_addrs[addr] = 1;

			return out;
		}

		void clearHits()
		{
			for (AddrToHitsMap_t::iterator it = m_addrs.begin();
					it != m_addrs.end();
					it++)
				it->second = 0;
		}

		unsigned int hits()
		{
			unsigned int out = 0;

			for (AddrToHitsMap_t::iterator it = m_addrs.begin();
					it != m_addrs.end();
					it++)
				out += it->second;

			return out;
		}

		unsigned int possibleHits()
		{
			return m_addrs.size();
		}

		uint8_t *marshal(uint8_t *start)
		{
			uint64_t *data = (uint64_t *)start;

			for (AddrToHitsMap_t::iterator it = m_addrs.begin();
					it != m_addrs.end();
					it++) {
				// Address and number of hits
				*data++ = (uint64_t)it->first;
				*data++ = (uint64_t)it->second;
			}

			return (uint8_t *)data;
		}

		static uint8_t *unMarshal(uint8_t *p,
				unsigned long *outAddr, unsigned int *outHits)
		{
			uint64_t *data = (uint64_t *)p;

			*outAddr = *data++;
			*outHits = *data++;

			return (uint8_t *)data;
		}

		std::string m_file;
		unsigned int m_lineNr;
		AddrToHitsMap_t m_addrs;
		unsigned int m_hits;
	};

	typedef std::unordered_map<LineId, Line *, LineIdHash> LineMap_t;
	typedef std::unordered_map<unsigned long, Line *> AddrToLineMap_t;

	LineMap_t m_lines;
	AddrToLineMap_t m_addrToLine;

	IElf &m_elf;
	ICollector &m_collector;

	unsigned int m_nrLines;
	unsigned int m_nrExecutedLines;
};

IReporter &IReporter::create(IElf &elf, ICollector &collector)
{
	return *new Reporter(elf, collector);
}
