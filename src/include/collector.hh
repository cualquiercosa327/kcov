#pragma once

namespace kcov
{
	class IElf;

	class ICollector
	{
	public:
		class IListener
		{
		public:
			virtual void onBreakpoint(unsigned long addr) = 0;
		};

		static ICollector &create(IElf *elf);

		virtual void registerListener(IListener &listener) = 0;


		virtual int run() = 0;
	};
}
