import './Toggle.css';

export interface ToggleProps {
  label?: string;
  value: boolean;
  onChange: (value: boolean) => void;
  /** Compact form for a panel header. */
  compact?: boolean;
}

export function Toggle({ label, value, onChange, compact }: ToggleProps) {
  return (
    <button
      type="button"
      className="gn-toggle"
      data-on={value}
      data-compact={compact}
      role="switch"
      aria-checked={value}
      aria-label={label ?? 'on'}
      onClick={() => onChange(!value)}
    >
      <span className="gn-toggle__led" />
      {label && !compact && <span className="gn-toggle__label">{label}</span>}
    </button>
  );
}
