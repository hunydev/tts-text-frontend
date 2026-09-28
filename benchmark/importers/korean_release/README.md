# User-provided Korean TN JSON release

Original stdlib importer for local `summary.json` plus category JSON files.
Preserves `ITN`, bracket-annotated `TN`, source topics, source hashes and row IDs.
Never imports references into product rules/lexicons or labels them human-approved.

See [Korean workflow and metric contracts](../../../docs/korean-reference-benchmark.ko.md).
Imported records use this source's schema (category/topic/alignment warnings), with
shared native Backend/UTF-8 span projection from the external benchmark evaluator.
Data and reports belong under ignored `benchmark/generated/`, never in Git.
