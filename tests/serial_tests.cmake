# Timings: tests running on the sibling cores slow these down threefold.
set_tests_properties(FrameCostTest.PageOfCardsRepaintsWithinAFrame PROPERTIES RUN_SERIAL TRUE)
