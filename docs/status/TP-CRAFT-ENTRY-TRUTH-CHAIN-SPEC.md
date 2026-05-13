# TP Craft Entry Truth Chain Spec

## Objective

- Define strict evidence chain for A6 pass (craft/install screen appears through real firmware flow).

## Required Chain

1. OOS stable before keypad sequence.
2. Hook state eligible (on-hook unless scenario explicitly differs).
3. TP ready sequence completed before code acceptance window.
4. Required security/service inputs active per profile.
5. TP reports expected craft key bytes (`2727378` path evidence).
6. CP consumes corresponding bytes in-order.
7. CP craft parser enters accepted state.
8. Craft gate accept event recorded.
9. Display queue receives craft message.
10. VFD renders craft screen text.

## Failure Classification

- First-failing-link classification is mandatory.
- Validator output must include:
  - per-link booleans
  - A6 aggregate result
  - classification tag

## Prohibited Evidence

- Direct display text injection.
- Manual CP buffer editing.
- Any bypass that sets craft-gate flags without protocol evidence.