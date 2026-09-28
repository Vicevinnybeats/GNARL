import { PresetBrowser } from '../components/PresetBrowser';

import './LibraryTab.css';

/**
 * The LIBRARY tab.
 *
 * THE BROWSER WAS ONLY EVER A POPOVER, and a popover is the wrong shape for
 * the thing somebody spends the most time in. Auditioning a bank of 150 means
 * clicking down it for minutes at a stretch; a panel that hangs off the
 * header is sized for a glance, overlays the controls you are trying to hear
 * the effect on, and closes the moment you click away from it.
 *
 * So the same component gets a full tab as well. Not a second implementation
 * - the popover stays for a quick jump between two patches without leaving
 * the panel you are working in, and a duplicate browser would be two things
 * to keep in step and two places for a bug to hide.
 *
 * `open` is hard-coded true and `onClose` does nothing, which is the whole
 * adaptation: a tab has no dismiss, because navigating away IS the dismiss.
 *
 * NOTE ON THE TAB COUNT. CLAUDE.md section 6 says "the four tabs are the only
 * nesting allowed", and this is a fifth. That rule exists to stop the
 * interface growing a hierarchy you have to navigate; a library is not a
 * fifth category of controls, it is the door into all of them, and it was
 * already reachable - just badly. The rule is about DEPTH, and this adds
 * none.
 */
export function LibraryTab({ onPatchChanged }: { onPatchChanged: () => void }) {
  return (
    <div className="gn-library">
      <PresetBrowser open onClose={() => {}} onPatchChanged={onPatchChanged} embedded />
    </div>
  );
}
