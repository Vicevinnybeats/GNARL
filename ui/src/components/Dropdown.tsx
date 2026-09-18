import './Dropdown.css';

export interface DropdownProps {
  label?: string;
  value: number;
  options: readonly string[];
  onChange: (index: number) => void;
  /** Stretches to fill its container. */
  wide?: boolean;
}

export function Dropdown({ label, value, options, onChange, wide }: DropdownProps) {
  return (
    <label className="gn-dropdown" data-wide={wide}>
      {label && <span className="gn-dropdown__label">{label}</span>}
      <select
        className="gn-dropdown__select"
        value={value}
        aria-label={label}
        onChange={(e) => onChange(Number(e.currentTarget.value))}
      >
        {options.map((option, index) => (
          <option key={option} value={index}>
            {option}
          </option>
        ))}
      </select>
    </label>
  );
}
