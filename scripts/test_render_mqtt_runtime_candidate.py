#!/usr/bin/env python3
"""Stdlib-only regression tests for render_mqtt_runtime_candidate.py."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import render_mqtt_runtime_candidate as renderer


def make_rule_fixture() -> bytes:
    """Build a synthetic exact-anchor fixture for renderer mechanics tests."""

    parts: list[str] = [
        "#include <PubSubClient.h>\n",
        "WiFiClient mqttNet;\nPubSubClient mqtt(mqttNet);\nWiFiUDP udp;\n",
        renderer.RULES[2].start,
        "\n// R03 fixture interior\n",
        renderer.RULES[2].end,
        "\n",
        renderer.RULES[3].start,
        "\n// R04 fixture interior\n",
        renderer.RULES[3].end,
        "\n",
        renderer.RULES[4].start,
        "\n// R05 fixture interior\n",
        renderer.RULES[4].end,
        "\n",
        renderer.RULES[5].start,
        "\n// R06 fixture interior\n",
        renderer.RULES[5].end,
        "\n",
        "  uint32_t now =\n"
        "      millis();\n",
        "\n",
        renderer.RULES[6].start,
        "\n// R07 fixture interior\n",
        renderer.RULES[6].end,
        "\n",
        renderer.RULES[7].start,
        "// R08 fixture interior\n",
        renderer.RULES[7].end,
        "\n// between R08 and R09\n",
        renderer.RULES[8].start,
        "\n// R09 fixture interior\n",
        renderer.RULES[8].end,
        "\n// R10 fixture interior\n",
        renderer.RULES[9].end,
        "\n",
        renderer.RULES[10].start,
        "\n// R11 fixture interior\n",
        renderer.RULES[10].end,
        "\n",
        renderer.RULES[11].start,
        "// R12 fixture interior\n",
        renderer.RULES[11].end,
        "\n",
        "  // ----------------------------------------------------------\n"
        "  // Wi-Fi\n"
        "  // ----------------------------------------------------------\n",
        "\n",
        renderer.RULES[12].start,
        "// R13 fixture interior\n",
        renderer.RULES[12].end,
        "\n",
        "constexpr uint32_t MAX_PUMP_SECONDS = 180;\n",
        "auto fixtureCommandBound = command_payload_policy::kMaxCommandPayloadBytes;\n",
        "void fixtureSafety() {\n",
        "  digitalWrite(\n    RELAY_PIN,\n    RELAY_OFF\n  );\n",
        "}\n",
    ]
    return "".join(parts).encode("utf-8")


class RendererTests(unittest.TestCase):
    def setUp(self) -> None:
        self.fixture = make_rule_fixture()
        self.original_blob = renderer.CANONICAL_MAIN_GIT_BLOB
        renderer.CANONICAL_MAIN_GIT_BLOB = renderer.git_blob_sha(self.fixture)

    def tearDown(self) -> None:
        renderer.CANONICAL_MAIN_GIT_BLOB = self.original_blob

    def test_rule_manifest_is_exactly_r01_through_r13(self) -> None:
        self.assertEqual(
            [rule.rule_id for rule in renderer.RULES],
            [f"R{number:02d}" for number in range(1, 14)],
        )

    def test_git_blob_identity_is_checked_before_render(self) -> None:
        wrong = self.fixture + b"\n"
        with self.assertRaisesRegex(renderer.RenderError, "blob mismatch"):
            renderer.render_bytes(wrong)

    def test_full_fixture_render_is_deterministic(self) -> None:
        first = renderer.render_bytes(self.fixture)
        second = renderer.render_bytes(self.fixture)
        self.assertEqual(first, second)
        self.assertEqual(renderer.sha256_hex(first), renderer.sha256_hex(second))
        rendered = first.decode("utf-8")
        for token in renderer.FORBIDDEN_TOKENS:
            self.assertNotIn(token, rendered)
        for marker in renderer.REQUIRED_MARKERS:
            self.assertIn(marker, rendered)

    def test_missing_anchor_fails_closed(self) -> None:
        rule = renderer.LiteralRule("RX", "needle", "replacement")
        with self.assertRaisesRegex(renderer.RenderError, "count must be 1"):
            rule.apply("haystack")

    def test_duplicated_anchor_fails_closed(self) -> None:
        rule = renderer.LiteralRule("RX", "needle", "replacement")
        with self.assertRaisesRegex(renderer.RenderError, "got 2"):
            rule.apply("needle needle")

    def test_block_rule_requires_unique_start_and_end(self) -> None:
        rule = renderer.BlockRule("RX", "START", "END", "NEW")
        with self.assertRaisesRegex(renderer.RenderError, "start anchor count"):
            rule.apply("START x START y END")
        with self.assertRaisesRegex(renderer.RenderError, "end anchor count"):
            rule.apply("START x END y END")

    def test_r06_end_anchor_is_wifi_scoped_with_duplicate_now_declaration(self) -> None:
        generic_now = "  uint32_t now =\n      millis();"
        fixture_text = self.fixture.decode("utf-8")
        self.assertEqual(2, fixture_text.count(generic_now))
        self.assertEqual(1, fixture_text.count(renderer.RULES[5].end))
        renderer.render_bytes(self.fixture)

    def test_r12_end_anchor_is_setup_scoped_with_duplicate_wifi_separator(self) -> None:
        generic_wifi_separator = (
            "  // ----------------------------------------------------------\n"
            "  // Wi-Fi\n"
            "  // ----------------------------------------------------------"
        )
        fixture_text = self.fixture.decode("utf-8")
        self.assertEqual(2, fixture_text.count(generic_wifi_separator))
        self.assertEqual(1, fixture_text.count(renderer.RULES[11].end))
        renderer.render_bytes(self.fixture)

    def test_already_rendered_input_fails_identity_gate(self) -> None:
        rendered = renderer.render_bytes(self.fixture)
        with self.assertRaisesRegex(renderer.RenderError, "blob mismatch"):
            renderer.render_bytes(rendered)

    def test_forbidden_residual_is_rejected(self) -> None:
        rendered = renderer.render_bytes(self.fixture).decode("utf-8")
        with self.assertRaisesRegex(renderer.RenderError, "forbidden legacy MQTT"):
            renderer.validate_rendered(rendered + "\nmqtt.state()\n")

    def test_urgent_stop_precedes_dynamic_string(self) -> None:
        rendered = renderer.render_bytes(self.fixture).decode("utf-8")
        self.assertLess(
            rendered.index("requestUrgentPumpStop(fromHA);"),
            rendered.index("String message(command);"),
        )

    def test_session_order_is_fail_closed(self) -> None:
        rendered = renderer.render_bytes(self.fixture).decode("utf-8")
        pump_sub = rendered.index("mqtt.startTrackedSubscription(T_PUMP_CMD)")
        cmd_sub = rendered.index("mqtt.startTrackedSubscription(T_CMD)")
        online = rendered.index('T_STATUS,\n            "online"')
        ready = rendered.index("mqttSessionInitState = MqttSessionInitState::Done;")
        telegram = rendered.index("serviceTelegramDelivery();")
        self.assertLess(pump_sub, cmd_sub)
        self.assertLess(cmd_sub, online)
        self.assertLess(online, ready)
        self.assertLess(ready, telegram)

    def test_output_path_must_differ_from_input(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "main.cpp"
            source.write_bytes(self.fixture)
            with self.assertRaisesRegex(renderer.RenderError, "overwrite"):
                renderer.render_file(source, source)

    def test_explicit_output_write_keeps_input_unchanged(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "main.cpp"
            output = Path(tmp) / "candidate" / "main.cpp"
            source.write_bytes(self.fixture)
            before = source.read_bytes()
            digest = renderer.render_file(source, output)
            self.assertEqual(source.read_bytes(), before)
            self.assertTrue(output.is_file())
            self.assertEqual(digest, renderer.sha256_hex(output.read_bytes()))

    def test_canonical_repository_source_renders_when_present(self) -> None:
        repo_root = SCRIPT_DIR.parent
        source = repo_root / "src" / "main.cpp"
        if not source.is_file():
            self.skipTest("repository src/main.cpp not present in this test environment")

        before = source.read_bytes()
        self.assertEqual(renderer.git_blob_sha(before), self.original_blob)
        renderer.CANONICAL_MAIN_GIT_BLOB = self.original_blob
        first = renderer.render_bytes(before)
        second = renderer.render_bytes(before)
        self.assertEqual(first, second)
        self.assertEqual(source.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
