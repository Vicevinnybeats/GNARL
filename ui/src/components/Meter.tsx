import './Meter.css';

export interface MeterProps {
  /** 0..1. */
  value: number;
  label?: string;
  /** Bipolar meters fill from the centre, for gain reduction. */
  bipolar?: boolean;
}

export function Meter({ value, label, bipolar }: MeterProps) {
  const clamped = Math.min(1, Math.max(bipolar ? -1 : 0, value));
  const magnitude = Math.abs(clamped) * (bipolar ? 50 : 100);

  return (
    <div className="gn-meter">
      {label && <span className="gn-meter__label">{label}</span>}
      <div className="gn-meter__track" data-bipolar={bipolar}>
        <span
          className="gn-meter__fill"
          style={
            bipolar
              ? {
                  width: `${magnitude}%`,
                  left: clamped < 0 ? `${50 - magnitude}%` : '50%',
                }
              : { width: `${magnitude}%`, left: 0 }
          }
        />
      </div>
    </div>
  );
}
