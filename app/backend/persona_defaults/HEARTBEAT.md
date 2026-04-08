# HEARTBEAT.md — Maintenance pass (hub)

The periodic heartbeat job sends you this file, recent daily logs, and current MEMORY.md. Your job in that pass:

1. Read **RECENT_DAILY_LOGS** and **CURRENT_MEMORY.md** in the user message.
2. Merge **durable** facts from the logs into MEMORY only: dedupe, drop ephemeral chatter unless the user asked to remember it.
3. Call **memory_replace** with the full updated MEMORY.md when it should change. Skip if MEMORY is already accurate.
4. Do **not** change SOUL.md, IDENTITY.md, USER.md, TOOLS.md, AGENTS.md, or HEARTBEAT.md during the maintenance pass.

Stay concise; the user does not see this turn unless you later speak proactively.

---

## Optional checklist (you may edit this section in normal chat via persona_replace)

- [ ] Review today's daily log for anything worth long-term MEMORY
- [ ] Note project or preference updates the user mentioned
