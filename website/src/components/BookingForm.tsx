"use client";

import { useState } from "react";

interface BookingFormProps {
  eventSlug: string;
  eventTitle: string;
  price: number;
}

export function BookingForm({ eventSlug, eventTitle, price }: BookingFormProps) {
  const [formData, setFormData] = useState({
    name: "",
    email: "",
    phone: "",
    participants: 1,
  });
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState("");

  const totalPrice = price * formData.participants;

  const formatPrice = (amount: number) =>
    new Intl.NumberFormat("nl-NL", {
      style: "currency",
      currency: "EUR",
    }).format(amount);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setIsLoading(true);
    setError("");

    try {
      const response = await fetch("/api/payments", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          eventSlug,
          eventTitle,
          ...formData,
          amount: totalPrice.toFixed(2),
        }),
      });

      const data = await response.json();

      if (!response.ok) {
        throw new Error(data.error || "Er ging iets mis bij het aanmaken van de betaling.");
      }

      // Redirect to Mollie checkout
      if (data.checkoutUrl) {
        window.location.href = data.checkoutUrl;
      }
    } catch (err) {
      setError(
        err instanceof Error
          ? err.message
          : "Er ging iets mis. Probeer het opnieuw."
      );
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <form onSubmit={handleSubmit} className="mt-6 space-y-4">
      <div>
        <label
          htmlFor="name"
          className="block text-sm font-medium text-tactical-300"
        >
          Naam *
        </label>
        <input
          type="text"
          id="name"
          required
          value={formData.name}
          onChange={(e) => setFormData({ ...formData, name: e.target.value })}
          className="mt-1 block w-full rounded-lg border border-tactical-700 bg-tactical-800 px-4 py-2.5 text-white placeholder-tactical-500 focus:border-accent focus:outline-none focus:ring-1 focus:ring-accent"
          placeholder="Je volledige naam"
        />
      </div>

      <div>
        <label
          htmlFor="email"
          className="block text-sm font-medium text-tactical-300"
        >
          E-mailadres *
        </label>
        <input
          type="email"
          id="email"
          required
          value={formData.email}
          onChange={(e) => setFormData({ ...formData, email: e.target.value })}
          className="mt-1 block w-full rounded-lg border border-tactical-700 bg-tactical-800 px-4 py-2.5 text-white placeholder-tactical-500 focus:border-accent focus:outline-none focus:ring-1 focus:ring-accent"
          placeholder="je@email.nl"
        />
      </div>

      <div>
        <label
          htmlFor="phone"
          className="block text-sm font-medium text-tactical-300"
        >
          Telefoonnummer *
        </label>
        <input
          type="tel"
          id="phone"
          required
          value={formData.phone}
          onChange={(e) => setFormData({ ...formData, phone: e.target.value })}
          className="mt-1 block w-full rounded-lg border border-tactical-700 bg-tactical-800 px-4 py-2.5 text-white placeholder-tactical-500 focus:border-accent focus:outline-none focus:ring-1 focus:ring-accent"
          placeholder="+31 6 12345678"
        />
      </div>

      <div>
        <label
          htmlFor="participants"
          className="block text-sm font-medium text-tactical-300"
        >
          Aantal deelnemers
        </label>
        <select
          id="participants"
          value={formData.participants}
          onChange={(e) =>
            setFormData({ ...formData, participants: Number(e.target.value) })
          }
          className="mt-1 block w-full rounded-lg border border-tactical-700 bg-tactical-800 px-4 py-2.5 text-white focus:border-accent focus:outline-none focus:ring-1 focus:ring-accent"
        >
          {[1, 2, 3, 4, 5, 6, 7, 8].map((n) => (
            <option key={n} value={n}>
              {n} {n === 1 ? "persoon" : "personen"}
            </option>
          ))}
        </select>
      </div>

      {/* Price summary */}
      <div className="rounded-xl bg-tactical-800/50 p-4">
        <div className="flex items-center justify-between text-sm text-tactical-400">
          <span>
            {formatPrice(price)} x {formData.participants}
          </span>
          <span className="text-lg font-bold text-white">
            {formatPrice(totalPrice)}
          </span>
        </div>
      </div>

      {error && (
        <div className="rounded-lg bg-red-500/10 p-3 text-sm text-red-400">
          {error}
        </div>
      )}

      <button
        type="submit"
        disabled={isLoading}
        className="btn-primary w-full disabled:cursor-not-allowed disabled:opacity-50"
      >
        {isLoading ? (
          <span className="flex items-center justify-center gap-2">
            <svg className="h-5 w-5 animate-spin" viewBox="0 0 24 24">
              <circle
                className="opacity-25"
                cx="12"
                cy="12"
                r="10"
                stroke="currentColor"
                strokeWidth="4"
                fill="none"
              />
              <path
                className="opacity-75"
                fill="currentColor"
                d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"
              />
            </svg>
            Bezig met verwerken...
          </span>
        ) : (
          `Betaal ${formatPrice(totalPrice)}`
        )}
      </button>

      <p className="text-center text-xs text-tactical-500">
        Veilig betalen via iDEAL, creditcard of andere methoden via Mollie
      </p>
    </form>
  );
}
