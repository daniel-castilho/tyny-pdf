# Decision log

Owner sanctions cited by `tasks/epic-01/epic-1-dod.md` (section 2, self-audit): a requirement or
report that claims an owner decision names the D-id and date below, verbatim to the answer the owner
gave. Deliberation notes in Portuguese live outside this repository; this file records only the
sanctions and their date, in English (ADR-0005).

## D-1 (2026-09-17) - Epic 1 executes one story at a time with a checkpoint

Owner decided the scope of the current working turn is Story 1.1 only, then a checkpoint before
Story 1.2. Answered in-session to the question "Escopo desta rodada?": "So a 1.1, depois
checkpoint". English recording: Story 1.1, then stop for review.

## D-2 (2026-09-17) - the epic documents stay in `tasks/epic-01/`

Owner decided the five epic documents keep their committed location instead of moving to
`docs/epics/`. Answered in-session: "Manter em tasks/epic-01/". Story 1.1 therefore corrects the
internal `docs/epics/` references to `tasks/epic-01/`; no file move happens.

## D-3 (2026-09-17) - the seed's three direct pushes are a recorded deviation, not rewritten history

Owner decided the seed commits (`9fc2d52`, `94f1950`, `0d64999`) on `main` stay as they are; Story
1.1 records the ADR-0009 bypass and the loss mechanism in `docs/lessons.md`, applies branch
protection, and every later change goes through a PR. Answered in-session: "Registrar desvio +
PR-flow daqui pra frente".

## D-4 (2026-09-17) - build prerequisites may be installed

Owner approved installing and downloading the toolchain prerequisites (CMake >= 3.30,
`clang-format`, `clang-tidy`, Conan 2, pinned LLVM-MinGW tarball) as Story 1.2 needs them.
Answered in-session: "Sim, instalar tudo".

## D-5 (2026-09-17) - a Windows host and reference machine exist for the Windows-only parts

Owner confirmed a Windows host and reference machine are available for `tools/win-probe/`, the
Story 1.4 Win32 window, and the Story 1.5 SumatraPDF baseline. Answered in-session: "Sim, tenho
host Windows + maquina de referencia".

## D-6 (2026-09-17) - the non-admin push rejection is recorded as owner-pending

Owner decided the direct-push rejection proof (Story 1.1 AC) is documented as a pending owner item
rather than fabricated with a second identity. Answered in-session: "Documentar como pendencia de
dono".