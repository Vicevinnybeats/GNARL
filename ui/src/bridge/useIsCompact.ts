import { useEffect, useState } from 'react';

/**
 * Whether the compact layout is in force.
 *
 * READ FROM THE DOM ATTRIBUTE rather than re-deriving it from the viewport.
 * `useAppScale` decides what compact means and writes `data-compact`; the
 * stylesheet reads that same attribute. A second copy of the rule here could
 * disagree with both, and a layout where the CSS thinks it is compact and the
 * components think it is not is worse than either answer on its own.
 *
 * Observed rather than polled, because the attribute changes exactly when
 * something writes it — on rotation, on resize — and a MutationObserver is
 * the notification the DOM already offers for that.
 */
export function useIsCompact(): boolean {
  const [compact, setCompact] = useState(
    () => typeof document !== 'undefined' && document.documentElement.dataset.compact === 'true',
  );

  useEffect(() => {
    const root = document.documentElement;
    const read = () => setCompact(root.dataset.compact === 'true');

    //  Once immediately: useAppScale's effect may have run after this
    //  component's initial state was computed.
    read();

    const observer = new MutationObserver(read);
    observer.observe(root, { attributes: true, attributeFilter: ['data-compact'] });

    return () => observer.disconnect();
  }, []);

  return compact;
}
