"""pytest plugin: collect record_property values into a JSON file (used by tools/measure.py)."""
import json

_rows = []


def pytest_addoption(parser):
    parser.addoption("--mm-report", default=None)


def pytest_runtest_logreport(report):
    if report.when != "call":
        return
    for name, value in getattr(report, "user_properties", []):
        _rows.append({"test": report.nodeid.split("::", 1)[-1], "name": name, "value": value, "outcome": report.outcome})


def pytest_sessionfinish(session, exitstatus):
    path = session.config.getoption("--mm-report")
    if path:
        with open(path, "w", encoding="utf-8") as f:
            json.dump(_rows, f, indent=1)
