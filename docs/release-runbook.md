# Tyny PDF — Operational Release Runbook

How to build, package, verify and publish **Tyny PDF** for Windows, and what has to be true
before a tag exists. Written before the first release on purpose: a runbook invented under
release pressure is where the shortcuts get made.

**Official Domain:** [https://tyny.ca](https://tyny.ca) | **App ID:** `ca.tyny.pdf`

> **Executable status: no.** There is no code, no build system, no CI history and no artefact
> today, so nothing in this file has ever been run end to end. Each step names what it waits on:
> the milestone in `kickoff.md`, the PR that writes the mechanism, or the open decision in
> section 9. A step that cannot be executed is still a commitment about how it will be executed -
> it is not a claim that it has been. The sibling project's runbook documents an auto-updater
> with a `pubkey` and an endpoint on `releases.tyny.ca`;
> [../adr/0006-product-name-and-identifier.md](../adr/0006-product-name-and-identifier.md)
> verified that host answers no records, and the updater it describes is not configured in that
> project's manifest either. That is the failure mode this file is written against: a release
> procedure must not become a description of infrastructure nobody built.

---

## 0. Release Artefacts and Targets

| Target                       | Artefact                                   | Toolchain                    | Notes                                                        |
| :--------------------------- | :----------------------------------------- | :--------------------------- | :----------------------------------------------------------- |
| Windows x64, portable        | `tynypdf-0.1.0-win-x64.zip`                | LLVM-MinGW cross from Linux  | the primary artefact; no installer, no admin rights          |
| Windows ARM64, portable      | same stem, `-win-arm64` suffix             | LLVM-MinGW AArch64           | built, not advertised (kickoff section 12)                   |
| Windows x64 installer (MSI)  | `TynyPDF-0.1.0-x64.msi`                    | MSVC parity build            | only when an enterprise ask exists (kickoff section 12)      |
| winget manifest              | `Tyny.TynyPDF` in `winget-pkgs`            | -                            | id availability is an open announcement gate (section 9)     |
| Checksums and detached sigs  | `SHA256SUMS`, `SHA256SUMS.asc`             | the release maintainer's key | ADR-0004 section 5; the `.asc` is free, the cert is not       |
| SBOM                         | CycloneDX, one per artefact set            | `tools/sbom.sh` (planned, PR #4) | generated at build time, never hand-edited                 |

Artefacts land next to the build directory the presets already use (`build/win-cross-x64/Release/`);
the packaging job that collects them is written with the CI matrix in PR #2.

**Not produced, deliberately:** no signed installer (see section 4), no `.appx`/Store package
(the Store is not pursued: the portable ZIP on GitHub Releases is the only channel, so nothing
re-signs the binary or weakens the no-installer promise), no updater payload, no `latest` symlink
that silently changes what a download URL points at.

---

## 1. Pre-flight: the gates that must be green

```bash
sh tools/check.sh                          # ten sections, all gates
python3 tools/spec-check.py                # pending ids must be zero before a tag
cmake --preset linux-core && cmake --build --preset linux-core      # from PR #2
ctest --preset linux-core --output-on-failure                        # from PR #5
cmake --preset win-cross-x64 && cmake --build --preset win-cross-x64
sh tools/bench-measure.sh --reference      # planned: PR #3; numbers within 10% of baseline
```

Release-blocking states, as they are today and until the milestone closes them:

- `src/features/sidecar/SPEC.md`: 10 requirements pending their verification artefact (R2.2, R2.3,
  R3.1, R3.2, R4.1, R4.2, R5.1, R5.2, R6.1, R6.2). M6's exit criterion is that they are green.
- Conformance: `tests/conformance/` must point at the pinned corpus submodule; the corpus repository
  does not exist yet, and a synthetic stand-in directory is forbidden (ADR-0010 rule 6).
- Redaction proof: the audit artefact from a text-extraction run on the redacted output must be
  produced by the build being released, not by an earlier one (kickoff M6).

---

## 2. Version, changelog and tag

1. One place declares the version. Until a manifest needs it, that place is the CMake project
   command, and `buildinfo.json` carries it into the binary (ADR-0004 section 5). Nothing in the
   tree duplicates it today; if PR #2 introduces a second copy, it must be generated, not
   hand-edited.
2. Rename `CHANGELOG.md`'s `[Unreleased]` section to `[0.1.0]` with the release date, keeping Keep a
   Changelog's headings. The changelog is generated from commit types and then read once by a human;
   a release note nobody proofread is a bug report waiting to happen (kickoff M6 exit).
3. No `__DATE__` or `__TIME__` anywhere in a shipped binary: a reproducible build has no build clock
   in it (ADR-0004 section 5, the pattern Sumatra's `BuiltOn` field uses).
4. Tag from `main` only, with `main` fully green:

   ```bash
   git switch main && git pull --ff-only
   git tag -a v0.1.0 -m "Tyny PDF 0.1.0"
   git push origin v0.1.0            # the tag is the release job's trigger (ADR-0009)
   ```

   `main` is the only long-lived branch and is always releasable; releases are tags
   (`docs/git-workflow.md`).

---

## 3. Build and package

The whole release build happens on Linux (ADR-0010): the same preset, the same pinned toolchain, the
same container image, so "works on my machine" has no room to appear.

```bash
cmake --preset win-cross-x64 -DCMAKE_BUILD_TYPE=Release && cmake --build --preset win-cross-x64
python3 tools/sbom.sh --out dist/cyclonedx.json          # planned: PR #4
sh tools/patch-report.sh --out dist/mupdf-patches.md     # planned: PR #4
```

Three properties make the artefact reviewable rather than trusted:

- **The engine is pinned by commit.** `third_party/UPSTREAM.toml` carries a 40-hex commit and
  `third_party/patches/` an ordered series; a release without a matching patch report is not
  released (ADR-0004).
- **The build is reproducible.** Static runtime, canonicalised source paths, stable file
  ordering, compiler pinned by hash through the CI container image tag; a rebuild on a second
  runner must produce the same bytes, and the licence file of the vendored engine ships inside
  the archive (AGPL-3.0).
- **The binary says what it is.** `buildinfo.json` is written by CI and read at runtime, so the
  about dialog can state commit, toolchain and patch count without a compile-time clock.

The portable ZIP is `tynypdf.exe` plus `LICENSE`, `THIRD-PARTY-NOTICES`, `buildinfo.json`, the
schema for the sidecar, and nothing else; no runtime redistributable, no write access outside its
own folder.

---

## 4. Signing: what we do, and what we say we do not

**Authenticode (the executable):** not present in v1. The release body states it in one sentence,
because an unstated gap is what a trust-oriented product cannot afford (ADR-0004 section 5). The
consequence is named rather than hidden: a first run on a fresh Windows machine shows a
reputation prompt, and SmartScreen will keep doing that until a signing decision is recorded in
an ADR. Two candidate paths are priced in that decision, not here: a purchased certificate with a
documented private-key procedure, or Trusted Signing with its monthly cost and its enrolment
requirements - both change the supply chain, so both are ADR-sized.

**PGP (the artefacts):** `SHA256SUMS.asc` is a detached signature by the release maintainer's
key, and it is free. It is offered as the integrity check that does not require trusting a
certificate authority, with the key fingerprint published in the repository rather than only on a
keyserver.

**PDF signatures (a feature, not our build):** the `PAdES-B-B (local)` label produced by the
viewer's signing flow is a signature *inside a document* by a *user's* key. It has nothing to do
with Authenticode on our binary, and a release note that blurs the two is a defect in this file.

---

## 5. Publishing

1. Create the GitHub Release as a **draft** against the tag; attach the artefacts, `SHA256SUMS`,
   `SHA256SUMS.asc`, the SBOM and the patch report. Nothing is published from a build machine
   directly.
2. Write the release body in this order: what a user can now do that they could not before, in the
   delta vocabulary of `docs/kickoff.md` (D-1 sidecar, D-2 forms, D-3 redaction proof, D-4 text, D-5
   accessibility, D-6 undo); what is not supported yet; the unsigned-binary sentence; the checksums.
3. Verify the checksums from the published assets on a clean machine before making the release
   public.
4. Submit the winget manifest only after the ZIP is public and its checksum is stable, since the
   manifest pins the URL and the hash.
5. `docs/naming.md` reserves a news feed path for release notes at
   `/releases/tynypdf/latest.json`. It may serve a *file the user asks for*; it may not be
   described as an update endpoint while `releases.tyny.ca` resolves to nothing (ADR-0006), and
   no build of this product may poll a host on its own - the default-deny rule (ADR-0003) has no
   release exception.
6. Un-draft. Then, and only then, announce; the announcement gates are DNS records and trademark
   clearance (kickoff sections 10 and 12).

---

## 6. Post-release smoke test on clean machines

Run on a Windows 11 VM with no developer tools, and again with a screen reader running:

1. Extract the ZIP into a fresh profile, launch, open a 1-page PDF. First paint without a network
   route (firewall set to block, then confirm the render is unaffected).
2. Add a highlight and a note, close, then check `document.pdf.tynypdf.json`: valid against
   `docs/sidecar.schema.json`, canonical bytes, and a two-byte edit is a two-line `git diff`.
3. Break the sidecar by hand (delete a comma) and confirm the app reports the corruption instead of
   showing zero annotations, and that the document itself is untouched.
4. Open a corrupt PDF and an encrypted one; each gets a specific message and a repair path, not a
   crash dialog (ADR-0003).
5. Fill a form with pt-BR text containing combining marks; validate, then flatten and re-open;
   `veraPDF-cli` on the flattened output reports no new errors.
6. Apply a redaction, then extract text from the output and confirm the redacted content is gone
   from the stream, not merely hidden; keep the audit artefact.
7. Sign with a generated test `.pfx`; re-open and confirm the label says `PAdES-B-B (local)` and
   that no network request was made; press "revalidate online" and confirm the disclosure names
   the destination first.
8. Run the CLI against the same document and compare exit codes: `0`, `1`, `2`, `3` each reachable,
   per [../adr/0003-error-model.md](../adr/0003-error-model.md) section 6.
9. Scroll 1000 pages at 250 % scaling and read the RSS: at or below 250 MB, and it must not grow
   when scrolling back. A `strace`/ETW capture during the whole session shows zero outbound
   sockets.
10. Delete the folder. If anything remains in the profile or the registry beyond the documented
    keys, the release is pulled and the portability claim is corrected (ADR-0010 and ADR-0006 own
    those keys).

---

## 7. Rollback and hotfix

- A defect on `main` is fixed forward. A published tag is never deleted or moved,
  since rewriting a tag makes an already-downloaded artefact look current: the answer to a bad
  release is `v0.1.1`.
- A defect in a shipped line follows `docs/git-workflow.md`: a `hotfix/*` branch starts from the
  tag, the fix is squash-merged there, then cherry-picked into `main` and forward-ported.
- A `release/0.x` branch is created only when a shipped line genuinely needs maintenance; one-off
  fixes do not earn a branch.
- Pulling an artefact means editing the release body to say it was pulled and why. Assets are not
  deleted: a checksum published anywhere in the wild is a fact about the past, and a missing file
  looks like an attack.
- A yank/`do-not-use` policy for a version that users may have pinned is **not decided**; it
  belongs in an ADR before 0.1.0, alongside the news feed's semantics.

---

## 8. What this runbook refuses to do

No silent auto-update and no unsigned delta patcher; no telemetry in a release build, including
"minimal install analytics"; no licence change, CLA or relicensing at release time; no claim that
a binary is trustworthy because a certificate says so; no build from a working tree rather than a
tag; no "quick" edit of a release note that changes a technical claim without changing the code
that makes it true.

---

## 9. Blockers, with the decision each one needs

| Blocker                                   | Owner / mechanism                                   | Needed before            |
| :---------------------------------------- | :-------------------------------------------------- | :----------------------- |
| No code, no build system, no CI matrix     | PR #2 wires the matrix and the build skeleton (see the header comment of `.github/workflows/gates.yml`); PR #5 fills it | any tag            |
| 10 SPEC requirements pending artefacts    | each delta's own PR; `tools/spec-check.py` prints the list | M6 exit          |
| Corpus repository for conformance         | create `daniel-castilho/tyny-pdf-corpus`, add as submodule at `tests/conformance/` (kickoff section 12) | first `0.1.0` candidate |
| Branch protection not enabled             | the six settings in `docs/git-workflow.md`, before PR #2 merges | a protected `main` |
| Authenticode decision (unsigned vs cert vs Trusted Signing) | an ADR with cost and key procedure; today's default is unsigned and stated | announcement |
| `docs/SWAP-CHECKLIST.md` filled in         | M6 exit criterion; the checklist is what makes "swappable engine" a tested property instead of a posture | M6 exit |
| DNS records (`www`, `updates.tyny.ca`)     | GoDaddy zone is empty; `releases.tyny.ca` must not be described as live until it resolves (ADR-0006) | announcement |
| Trademark clearance for "Tyny"; `Tyny.TynyPDF` free in winget-pkgs | owner action; announcement gates, not development gates (kickoff section 12) | announcement |

**Decided, not a blocker:** the Windows Store / MSIX channel is **not pursued** (kickoff section
12, decided 2026-09-17). A Store listing would force re-signing and change the portable-binary
promise, so the portable ZIP attached to a GitHub Release is the only distribution channel; no
step in this runbook waits on a store submission.

Where a row says "planned" or "open", the release is not late: the release is blocked on a
decision that has a name, a mechanism and a deadline. That is the difference between a blocker
and a surprise.

---

> _"A release is the tag plus the evidence, not the tag plus the binaries. If a step here cannot be
> executed, the fix is the mechanism it is waiting on - never the sentence that describes it."_
