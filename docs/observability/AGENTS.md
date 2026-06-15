# Observability Docs Agent Notes

These docs are for designing a future `rpgkit` observability API from the
concrete Unreal workshop demo.

Rules:

1. Keep examples grounded in the current demo scenario: Expose Weakness, Strike,
   Rend/Bleed, EndTurn, Goblin Attack, block cleanup.
2. Distinguish gameplay events from observable events.
3. Do not turn observations into gameplay drivers.
4. Prefer structured records over prose-only combat logs.
5. Track what belongs in `rpgkit` core versus `rpgkit-ue` host adapter.
6. Keep minimum useful v1 small.
7. When adding new observations, explain the debugging question they answer.
