import { useState } from "react";

import type { ReactNode } from "react";

type CollapsiblePanelProps = {
  title: string;
  subtitle?: string;
  badge?: ReactNode;
  defaultOpen?: boolean;
  children: ReactNode;
};

export function CollapsiblePanel({
  title,
  subtitle,
  badge,
  defaultOpen = true,
  children,
}: CollapsiblePanelProps) {
  const [isOpen, setIsOpen] = useState(defaultOpen);

  return (
    <details
      className="collapsible-panel"
      open={isOpen}
      onToggle={(event) => setIsOpen(event.currentTarget.open)}
    >
      <summary className="collapsible-panel-summary">
        <div>
          <h3>{title}</h3>
          {subtitle && <p>{subtitle}</p>}
        </div>

        <div className="collapsible-panel-right">
          {badge}
          <span className="collapsible-chevron">⌄</span>
        </div>
      </summary>

      <div className="collapsible-panel-body">{children}</div>
    </details>
  );
}