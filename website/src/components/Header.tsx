"use client";

import { useState } from "react";
import Link from "next/link";

const navLinks = [
  { href: "/#over-ons", label: "Over Ons" },
  { href: "/#banano-pro", label: "Banano Pro 2" },
  { href: "/events", label: "Events & Training" },
  { href: "/#faciliteit", label: "Faciliteit" },
  { href: "/#contact", label: "Contact" },
];

export function Header() {
  const [isOpen, setIsOpen] = useState(false);

  return (
    <header className="fixed top-0 z-50 w-full border-b border-tactical-800/50 bg-tactical-950/90 backdrop-blur-md">
      <nav className="mx-auto flex max-w-7xl items-center justify-between px-4 py-4 sm:px-6 lg:px-8">
        {/* Logo */}
        <Link href="/" className="flex items-center gap-3">
          <div className="flex h-10 w-10 items-center justify-center rounded-lg bg-accent font-heading text-xl font-bold">
            RTT
          </div>
          <div className="hidden sm:block">
            <div className="font-heading text-lg font-bold leading-tight text-white">
              Running The Target
            </div>
            <div className="text-xs text-tactical-400">
              Tactical Airsoft Simulations
            </div>
          </div>
        </Link>

        {/* Desktop Nav */}
        <div className="hidden items-center gap-1 md:flex">
          {navLinks.map((link) => (
            <Link
              key={link.href}
              href={link.href}
              className="rounded-lg px-4 py-2 text-sm font-medium text-tactical-300 transition-colors hover:bg-tactical-800 hover:text-white"
            >
              {link.label}
            </Link>
          ))}
          <Link href="/events" className="btn-primary ml-4 text-sm">
            Boek Nu
          </Link>
        </div>

        {/* Mobile menu button */}
        <button
          onClick={() => setIsOpen(!isOpen)}
          className="rounded-lg p-2 text-tactical-300 hover:bg-tactical-800 md:hidden"
          aria-label="Menu openen"
        >
          <svg className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            {isOpen ? (
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
            ) : (
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 6h16M4 12h16M4 18h16" />
            )}
          </svg>
        </button>
      </nav>

      {/* Mobile Nav */}
      {isOpen && (
        <div className="border-t border-tactical-800 bg-tactical-950 md:hidden">
          <div className="space-y-1 px-4 py-4">
            {navLinks.map((link) => (
              <Link
                key={link.href}
                href={link.href}
                onClick={() => setIsOpen(false)}
                className="block rounded-lg px-4 py-3 text-base font-medium text-tactical-300 transition-colors hover:bg-tactical-800 hover:text-white"
              >
                {link.label}
              </Link>
            ))}
            <Link
              href="/events"
              onClick={() => setIsOpen(false)}
              className="btn-primary mt-4 block w-full text-center"
            >
              Boek Nu
            </Link>
          </div>
        </div>
      )}
    </header>
  );
}
