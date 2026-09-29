import { useState, type ReactNode } from 'react';
import { useIsCompact } from '../bridge/useIsCompact';
import './CompactSections.css';

export interface CompactSection {
  /** Stable key, and what the button reads. */
  id: string;
  label: string;
  content: ReactNode;
}

/**
 * Splits a tab into sub-tabs, but only on a phone.
 *
 * WHY THIS IS NOT JUST A TAB COMPONENT. On a desktop every section is on
 * screen at once and that is the point — a synthesizer is read by comparing
 * one panel against another, and hiding two thirds of it behind buttons
 * would be a worse interface, not a smaller one. So when the layout is not
 * compact this renders every section exactly as if it were not here.
 *
 * On a phone the same content is a scroll of several screens, and a scroll
 * is a worse way to find one of four things than four labels are.
 *
 * NOTHING IS UNMOUNTED. The inactive sections are hidden, not removed,
 * because their state is real: the FX rack holds which slot is selected and
 * has fetched the chain order from the engine, and the MOD tab holds curves
 * and destinations it fetched once on mount. Unmounting would throw those
 * away and re-fetch them every time somebody looked at another section.
 *
 * The wrapper uses `display: contents` so that it disappears from layout
 * entirely when not hidden — without it, every grid and flex rule in the
 * tabs would be addressing this div instead of the panels inside it.
 */
export function CompactSections({ sections }: { sections: CompactSection[] }) {
  const compact = useIsCompact();
  const [active, setActive] = useState(sections[0]?.id ?? '');

  //  A section can disappear between renders; falling back to the first
  //  keeps something on screen rather than nothing.
  const current = sections.some((s) => s.id === active) ? active : sections[0]?.id;

  return (
    <>
      {compact && (
        <div className="gn-subtabs" role="tablist">
          {sections.map((section) => (
            <button
              key={section.id}
              type="button"
              role="tab"
              aria-selected={section.id === current}
              className={
                section.id === current ? 'gn-subtabs__tab gn-subtabs__tab--on' : 'gn-subtabs__tab'
              }
              onClick={() => setActive(section.id)}
            >
              {section.label}
            </button>
          ))}
        </div>
      )}

      {sections.map((section) => (
        <div key={section.id} className="gn-section" hidden={compact && section.id !== current}>
          {section.content}
        </div>
      ))}
    </>
  );
}
