import type { ReactNode } from 'react';

import './Panel.css';

export interface PanelProps {
  title?: string;
  /** Right-aligned content in the header, e.g. an enable toggle. */
  headerRight?: ReactNode;
  /** Dims the panel when its section is switched off. */
  dimmed?: boolean;
  /** Fills the available height rather than hugging its content. */
  grow?: boolean;
  children: ReactNode;
}

export function Panel({ title, headerRight, dimmed, grow, children }: PanelProps) {
  return (
    <section className="gn-panel" data-dimmed={dimmed} data-grow={grow}>
      {(title || headerRight) && (
        <header className="gn-panel__head">
          {title && <span className="gn-panel__title">{title}</span>}
          {headerRight && <span className="gn-panel__right">{headerRight}</span>}
        </header>
      )}
      <div className="gn-panel__body">{children}</div>
    </section>
  );
}

/** A labelled row of controls inside a panel. */
export function Row({ children, gap }: { children: ReactNode; gap?: 'tight' | 'wide' }) {
  return (
    <div className="gn-row" data-gap={gap}>
      {children}
    </div>
  );
}
