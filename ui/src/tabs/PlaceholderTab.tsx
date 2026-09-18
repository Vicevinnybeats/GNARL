import { Panel } from '../components/Panel';
import './PlaceholderTab.css';

export interface PlaceholderTabProps {
  title: string;
  phase: string;
  summary: string;
  items: readonly string[];
}

/**
 * A tab whose feature is not built yet.
 *
 * Says explicitly what is coming and when, rather than showing an empty panel.
 * An empty panel reads as a bug; a list of what will be here reads as a
 * roadmap, and it costs nothing.
 */
export function PlaceholderTab({ title, phase, summary, items }: PlaceholderTabProps) {
  return (
    <div className="gn-placeholder-tab">
      <Panel title={`${title} — ${phase}`} grow>
        <p className="gn-placeholder-tab__summary">{summary}</p>
        <ul className="gn-placeholder-tab__list">
          {items.map((item) => (
            <li key={item}>{item}</li>
          ))}
        </ul>
      </Panel>
    </div>
  );
}
