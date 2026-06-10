import e4_irredundancy as e4

def test_e4_all_witnesses_irredundant():
    rows = e4.run()
    assert len(rows) == 5
    for r in rows:
        assert r["differ"], f"{r['target']} did not change H^P"
        assert r["match"],  f"{r['target']} did not reproduce fixture value"
