"""Compare engine-relative trace ordinals under CJK design section 9.1.

Use one audit per comparison run and one group per engine configuration. Only
declaration_order may differ; finalize() must be checked after every observation.
Original captures are never edited. This does not change runtime tie-breaking.
"""
from collections import defaultdict


class TraceOrderAudit:
    def __init__(self):
        self.groups = defaultdict(set)
        self.shifts = []

    def align(self, engine, before, after, case):
        if len(before) != len(after):
            raise ValueError("trace event count changed")
        aligned = []
        for index, (old, new) in enumerate(zip(before, after)):
            if {k: v for k, v in old.items() if k != "declaration_order"} != {
                    k: v for k, v in new.items() if k != "declaration_order"}:
                raise ValueError(f"trace[{index}]: fields other than declaration_order changed")
            a, b = old.get("declaration_order"), new.get("declaration_order")
            if a is None or b is None:
                if a != b:
                    raise ValueError(f"trace[{index}]: resource ordinal presence changed")
            else:
                if type(a) is not int or type(b) is not int or min(a, b) < 0:
                    raise ValueError(f"trace[{index}]: invalid resource ordinal")
                self.groups[engine, old.get("layer")].add((a, b))
                if a != b:
                    self.shifts.append(dict(case=case, event=index, layer=old.get("layer"),
                                            before=a, after=b, offset=b - a))
            aligned.append(dict(new, declaration_order=a))
        return aligned

    def finalize(self):
        errors = []
        for (engine, layer), pairs in self.groups.items():
            ordered = sorted(pairs)
            # Preserve ties as well as ordering, within the same engine/layer.
            relative = all((a < c) == (b < d) and (a == c) == (b == d)
                           for (a, b), (c, d) in zip(ordered, ordered[1:]))
            offsets = {b - a for a, b in pairs}
            if not relative or len(offsets) != 1:
                errors.append(dict(engine=engine, layer=layer,
                                   relative_order_preserved=relative, offsets=sorted(offsets)))
        return errors
