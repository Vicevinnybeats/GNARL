import type { LicenseState } from '../bridge/license';

import './LicenseBanner.css';

export interface LicenseBannerProps {
  state: LicenseState;
  onDismiss: () => void;
}

/**
 * The licence banner. Lives in the status-bar row.
 *
 * WHY THERE AND NOT IN A ROW OF ITS OWN: the tab layout's vertical budget is
 * exact — 720 minus the 58 px header, the 22 px status bar and 16 px of
 * padding leaves 624 px, and a panel taller than its track renders *on top
 * of* its siblings. That has bitten this interface three times (CLAUDE.md
 * section 6). A banner that can be up for thirty days must not cost the
 * panels 24 px, and an overlay would cover controls instead. The status bar
 * is where the state of the plugin already lives.
 *
 * WHAT IT MUST NEVER SAY is the other constraint: nothing here suggests the
 * plugin has stopped making sound, because it has not and it cannot
 * (CLAUDE.md section 9). The expired wording names what still works first —
 * somebody reading it is mid-take and worried about losing it.
 *
 * The words come from C++ (`license::describe`) so they are versioned with
 * the policy they describe rather than drifting from it.
 */
export function LicenseBanner({ state, onDismiss }: LicenseBannerProps) {
  return (
    <span className="gn-license" data-status={state.status} role="status">
      <span className="gn-license__text">{state.message}</span>
      <button
        className="gn-license__close"
        type="button"
        aria-label="Dismiss licence notice"
        title="Dismiss"
        onClick={onDismiss}
      >
        ×
      </button>
    </span>
  );
}
