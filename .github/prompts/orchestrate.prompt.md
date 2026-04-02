---
description: "Start an orchestrated plan-execute-review workflow for an embedded-infra-lib development task. Routes through planning (Claude Opus 4.6), implementation, and code review stages with handoff buttons between each step."
agent: "orchestrator"
argument-hint: "Describe the feature, bug fix, or change you want to implement"
model: "Claude Sonnet 4"
---

Analyze the following task for the embedded-infra-lib (EmIL) project. Gather relevant context from the codebase — identify affected modules, existing patterns, and related interfaces. Then provide a brief scope summary and use the handoff buttons to route to the appropriate specialist:

- **Plan Implementation**: For complex tasks needing detailed upfront design
- **Execute Directly**: For straightforward changes with a clear path
- **Review Code**: For reviewing existing or recently changed code

Task to orchestrate:
