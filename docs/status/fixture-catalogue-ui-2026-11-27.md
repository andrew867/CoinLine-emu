# Fixture catalogue UI labels

The portable launcher generates per-run artwork using the resolved magnetic
and contact fixture order. Both views display the same one-based slot and
fixture-label rules. Known package fixtures have descriptive labels;
custom fixtures retain a neutral slot label with the basename recorded
in the run-local catalogue report. The original native selection output
and insertion controls are unchanged.

The 27 portable-profile tests passed. They cover both automatic contact
catalogues, selected-slot labels, the magnetic alternative, the neutral
custom fallback, and preservation of the source artwork.

The existing native executable
(SHA-256 `b9f3752f9db4334cf55e0ca6141184cbe6b3e8461be22b84f7903aec03e3864f`)
loaded the generated artwork in isolated runs. A test-only view marker
confirmed the external artwork was actually loaded, rather than the
compiled-in fallback. The earlier mapping cycled all six configured
contact slots and the later mapping all five. Insertion, removal, reader
presence, selection lock while inserted, and catalogue wrap passed.

Local evidence:
`build/firmware-acceptance/fixture-catalogue-ui/20261128T032657.023508Z/`.
These tests qualify catalogue presentation and native UI contact signals,
not an application or authorization path.
