"""The trace gate must reject changed resources and nonuniform/reversed orders."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/evaluation"))
from trace_order import TraceOrderAudit


def event(order, **fields):
    return dict(declaration_order=order, kind="lexicon", language="ko-KR",
                resource_language="ko-KR", key="en-US:etf", value="이티에프",
                layer="domain", disposition="selected", stage="lexicon", **fields)


class TraceOrderTests(unittest.TestCase):
    def test_uniform_offset_and_unchanged_other_layer(self):
        audit = TraceOrderAudit()
        old = [event(4), event(9), event(4)]
        new = [event(305), event(310), event(305)]
        self.assertEqual(audit.align("engine", old, new, "case"), old)
        audit.align("engine", [dict(event(2), layer="system")],
                    [dict(event(2), layer="system")], "system")
        self.assertEqual(audit.finalize(), [])
        self.assertEqual({s["offset"] for s in audit.shifts}, {301})
        self.assertEqual(new[0]["declaration_order"], 305)

    def test_every_nonordinal_field_is_strict(self):
        old = event(4, part_begin=0)
        for field in old.keys() - {"declaration_order"}:
            with self.subTest(field=field), self.assertRaises(ValueError):
                TraceOrderAudit().align("e", [old], [dict(old, **{field: "changed"})], "c")

    def test_nonuniform_offsets_across_cases_fail(self):
        audit = TraceOrderAudit()
        audit.align("e", [event(4)], [event(305)], "first")
        audit.align("e", [event(9)], [event(311)], "second")
        self.assertTrue(audit.finalize())

    def test_inversion_and_broken_ties_fail(self):
        for orders in ((310, 305), (305, 305)):
            audit = TraceOrderAudit()
            audit.align("e", [event(4), event(9)], list(map(event, orders)), "c")
            self.assertTrue(audit.finalize())

    def test_missing_ordinal_and_event_fail(self):
        for after in ([], [event(None)]):
            with self.assertRaises(ValueError):
                TraceOrderAudit().align("e", [event(4)], after, "c")


if __name__ == "__main__":
    unittest.main()
