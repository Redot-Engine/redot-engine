# AI Policy

## Purpose

This document is intended to deliver guidance on the usage of AI tooling and systems in regards to the development of Redot Engine.
Given the continued adoption in the industry, it is necessary that we outline where and how the technology is used in relation to the project.

### Why?

The purpose of this policy is to ensure that AI tools are used responsibly and ethically within the Redot Engine project.
AI tools are now integrated into most development tools and workflows. LLMs are a tool, just like any other. And like any other tool can be misused.
What we care about is the resulting code quality, not what tools a developer used in the process.
We do not believe that it is productive to police what development tools contributors use in their workflows,
or that a blanket ban on generative AI is productive or remotely enforceable. We believe that it is better to put in place
sensible guidelines governing the usage of such tools instead to ensure that they are used responsibly.


### Scope

This document is applicable to the following areas:
- The Engine repo, including supporting ones such as [Redot-cpp](https://github.com/Redot-Engine/redot-cpp)
- Supporting documentation
- Websites and services supporting the engine

---

## Usage

- Contributors are permitted to use LLMs / Agentic Coding Assistants in their workflows.
- Contributors are expected to understand the code they submit, and are fully responsible for it.
- Contributors are expected to ensure that any comments or supporting documentation is correct and accurate.
- Contributors are expected to manually test their PRs without the use of AI before submission to verify they actually work prior to code review.
- Contributors are expected to be able to answer technical questions about their code during code review, and justify architectural decisions.*
- AI-generated code is subject to the same quality standards as human-written code.

*Note: While AI may be used to dictate direction for several viable options, the contributor should still perform due diligence
to research the options themselves even if they use AI to aggregate pros and cons and inform them of the options and inner workings.

### Review

- Code Rabbit is used for prescreening PRs, and is meant to supplement human code review, not replace it.
- All code is to be thoroughly reviewed by human maintainers.
- All PRs are to be tested by a maintainer locally before merge.
- Maintainers may reject any PR that does not meet our quality standards, regardless of whether it was made by AI or a human.

### Maintainer Discretion

Project maintainers reserve the right to:

- Request clarification regarding AI-assisted contributions.
- Require revisions or additional testing.
- Reject low-quality or unverifiable submissions.
- Remove spam or abusive AI-generated content.

---

## Prohibited Usage

While AI usage is generally permitted in many cases, this section outlines restrictions on how generative AI may be used.
Contributors violating this section of the policy may be barred from making future contributions to the project.

### Disallowed Examples

- Submitting code that the contributor does not understand. (AKA Vibe coding)
- Submitting unreviewed or untested AI-generated output.
- Mass-generated or low-effort pull requests.
- AI-generated spam in issues or discussions.
- Submitting generated content with unclear licensing provenance.
- Using AI tools to generate security-sensitive code without rigorous review.
- Misrepresenting AI-generated work as entirely human-authored when disclosure is requested or required.

### Artwork & Graphics

Purely AI-generated artwork and graphics are not permitted anywhere in this repository or places governed by this document.
If AI is used anywhere in the process for creation of art assets, it must be disclosed upon submission along with a detailed explanation of how it was used.
Maintainers will make a determination on if a particular work constitutes enough human authorship for inclusion.
Generally the larger the role AI played in the creation of the work, the less likely it is to be accepted.

*Note, this section of the policy does not modify any other existing contractual agreements or terms a contributor may have with the Redot Foundation.

---

## Licensing

Contributors must not submit content that violates copyright, license terms, or third-party usage restrictions.

Because the provenance of AI-generated output may be unclear, contributors are responsible for ensuring:

- Submitted material is legally distributable.
- Compatible with the project license.
- Does not knowingly reproduce proprietary or copyrighted material.

If licensing status cannot be reasonably verified, the contribution may be rejected.

---

## Future Revisions

This policy may be updated over time as AI tooling, legal guidance, and community standards evolve.

Contributors are encouraged to review the latest version of this policy before submitting changes.
