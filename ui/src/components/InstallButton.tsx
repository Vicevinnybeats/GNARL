import { useEffect, useState } from 'react';
import './InstallButton.css';

/*  Chromium fires this instead of showing its own prompt, and hands over a
    `prompt()` that may be called ONCE, from a user gesture. It is not in
    lib.dom, so it is declared here rather than cast away at the call site. */
interface InstallPromptEvent extends Event {
  prompt: () => Promise<void>;
  userChoice: Promise<{ outcome: 'accepted' | 'dismissed' }>;
}

/**
 * A one-tap install, instead of a menu somebody has to know about.
 *
 * ON A PHONE "INSTALLER" MEANS THIS. There is no VST3 to install; the app
 * installs by being added to the home screen, which until now was buried in
 * the browser's own menu behind a name that varies per browser ("Add to Home
 * screen", "Install app", "Install"). Somebody who has been handed a link
 * has no reason to look there.
 *
 * NOT ALWAYS AVAILABLE, and the button says so by not existing. Chromium
 * fires `beforeinstallprompt`; iOS Safari does not implement it at all and
 * never will, so on an iPhone the button is replaced by the one instruction
 * that actually works there. A button that does nothing when tapped is worse
 * than no button.
 */
export function InstallButton() {
  const [deferred, setDeferred] = useState<InstallPromptEvent | null>(null);
  const [installed, setInstalled] = useState(false);
  const [iosHint, setIosHint] = useState(false);

  useEffect(() => {
    //  Already running as an app: nothing to offer.
    const standalone =
      window.matchMedia('(display-mode: standalone)').matches ||
      window.matchMedia('(display-mode: fullscreen)').matches ||
      (navigator as unknown as { standalone?: boolean }).standalone === true;

    if (standalone) {
      setInstalled(true);
      return undefined;
    }

    /*  iOS has no install event. Detected by the ONE combination that is
        actually diagnostic - a touch-capable device reporting a WebKit
        platform - rather than by sniffing the user-agent string, which
        every browser lies in. */
    const ios =
      /iPad|iPhone|iPod/.test(navigator.platform) ||
      (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);

    if (ios) setIosHint(true);

    const onPrompt = (event: Event) => {
      //  Suppress the browser's own banner so the offer appears where the
      //  rest of the interface is, not over it.
      event.preventDefault();
      setDeferred(event as InstallPromptEvent);
    };

    const onInstalled = () => {
      setInstalled(true);
      setDeferred(null);
    };

    window.addEventListener('beforeinstallprompt', onPrompt);
    window.addEventListener('appinstalled', onInstalled);

    return () => {
      window.removeEventListener('beforeinstallprompt', onPrompt);
      window.removeEventListener('appinstalled', onInstalled);
    };
  }, []);

  if (installed) return null;

  if (deferred !== null) {
    return (
      <button
        type="button"
        className="gn-install"
        onClick={() => {
          void deferred.prompt().then(() => {
            //  The event is single-use: once prompted it cannot be prompted
            //  again, so it goes whatever the person chose.
            setDeferred(null);
          });
        }}
      >
        Install
      </button>
    );
  }

  if (iosHint) {
    return (
      <span className="gn-install gn-install--hint">
        Share → Add to Home Screen
      </span>
    );
  }

  return null;
}
