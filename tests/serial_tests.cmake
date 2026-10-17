# Timings: tests running on the sibling cores slow these down threefold.
set_tests_properties(FrameCostTest.PageOfCardsRepaintsWithinAFrame
                     LayoutCostTest.SettingsResize
                     LayoutCostTest.SettingsValueChange
                     LayoutCostTest.SettingsScrollAndHover
                     LayoutCostTest.LongList
                     LayoutCostTest.CellLayoutSolve
                     LayoutCostTest.BoxLayoutMeasure
                     PROPERTIES RUN_SERIAL TRUE)
