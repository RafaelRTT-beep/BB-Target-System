import Link from "next/link";
import { getUpcomingEvents, formatPrice, formatDate } from "@/lib/events";
import { createMetadata } from "@/lib/seo";

export const metadata = createMetadata({
  title: "Events & Trainingen",
  description:
    "Bekijk alle aankomende airsoft trainingen, tactische events, competities en workshops bij Running The Target in Den Haag. Boek direct online met iDEAL.",
  path: "/events",
  keywords: [
    "airsoft events den haag",
    "tactische training boeken",
    "airsoft competitie nederland",
    "CQB training inschrijven",
    "airsoft workshop",
    "teambuilding airsoft",
  ],
});

const categoryColors: Record<string, string> = {
  training: "bg-blue-500/10 text-blue-400 border-blue-500/20",
  event: "bg-purple-500/10 text-purple-400 border-purple-500/20",
  competitie: "bg-red-500/10 text-red-400 border-red-500/20",
  workshop: "bg-amber-500/10 text-amber-400 border-amber-500/20",
};

export default function EventsPage() {
  const events = getUpcomingEvents();

  return (
    <div className="pt-24">
      {/* Header */}
      <section className="py-16">
        <div className="mx-auto max-w-7xl px-4 sm:px-6 lg:px-8">
          <div className="max-w-2xl">
            <h1 className="section-heading">
              Events & <span className="gradient-text">Trainingen</span>
            </h1>
            <p className="section-subheading">
              Van basistrainingen tot advanced competities. Schrijf je in en
              betaal veilig online via iDEAL, creditcard of andere betaalmethoden.
            </p>
          </div>
        </div>
      </section>

      {/* Events Grid */}
      <section className="pb-24">
        <div className="mx-auto max-w-7xl px-4 sm:px-6 lg:px-8">
          {events.length === 0 ? (
            <div className="rounded-2xl border border-tactical-800 bg-tactical-900/50 p-12 text-center">
              <h3 className="font-heading text-xl font-semibold text-white">
                Geen aankomende events
              </h3>
              <p className="mt-2 text-tactical-400">
                Er zijn momenteel geen events gepland. Neem contact op voor
                meer informatie.
              </p>
            </div>
          ) : (
            <div className="grid gap-8 lg:grid-cols-2">
              {events.map((event) => (
                <Link
                  key={event.slug}
                  href={`/events/${event.slug}`}
                  className="card group flex flex-col"
                >
                  <div className="flex items-start justify-between gap-4">
                    <div
                      className={`inline-flex rounded-full border px-3 py-1 text-xs font-medium capitalize ${categoryColors[event.category] || ""}`}
                    >
                      {event.category}
                    </div>
                    {event.spotsLeft <= 5 && (
                      <span className="inline-flex rounded-full bg-red-500/10 px-3 py-1 text-xs font-medium text-red-400">
                        Nog {event.spotsLeft} plekken!
                      </span>
                    )}
                  </div>

                  <h2 className="mt-4 font-heading text-2xl font-bold text-white transition-colors group-hover:text-accent">
                    {event.title}
                  </h2>
                  <p className="mt-2 flex-1 text-tactical-400">
                    {event.description}
                  </p>

                  {/* Features */}
                  <div className="mt-4 flex flex-wrap gap-2">
                    {event.features.map((feature) => (
                      <span
                        key={feature}
                        className="rounded-lg bg-tactical-800 px-2 py-1 text-xs text-tactical-300"
                      >
                        {feature}
                      </span>
                    ))}
                  </div>

                  {/* Event details */}
                  <div className="mt-6 grid grid-cols-2 gap-4 border-t border-tactical-800 pt-4 sm:grid-cols-4">
                    <div>
                      <div className="text-xs text-tactical-500">Datum</div>
                      <div className="mt-0.5 text-sm font-medium text-white">
                        {formatDate(event.date)}
                      </div>
                    </div>
                    <div>
                      <div className="text-xs text-tactical-500">Tijd</div>
                      <div className="mt-0.5 text-sm font-medium text-white">
                        {event.time} ({event.duration})
                      </div>
                    </div>
                    <div>
                      <div className="text-xs text-tactical-500">Locatie</div>
                      <div className="mt-0.5 text-sm font-medium text-white">
                        {event.location}
                      </div>
                    </div>
                    <div>
                      <div className="text-xs text-tactical-500">Prijs</div>
                      <div className="mt-0.5 text-lg font-bold text-accent">
                        {formatPrice(event.price)}
                      </div>
                    </div>
                  </div>
                </Link>
              ))}
            </div>
          )}
        </div>
      </section>

      {/* Teambuilding CTA */}
      <section className="border-t border-tactical-800 bg-tactical-900/50 py-16">
        <div className="mx-auto max-w-7xl px-4 text-center sm:px-6 lg:px-8">
          <h2 className="font-heading text-2xl font-bold text-white sm:text-3xl">
            Op zoek naar een teambuilding op maat?
          </h2>
          <p className="mx-auto mt-4 max-w-xl text-tactical-400">
            We organiseren ook maatwerk events voor bedrijven, vrijgezellenfeesten
            en groepen. Neem contact op voor de mogelijkheden.
          </p>
          <a href="tel:+31641102662" className="btn-primary mt-8 inline-flex">
            Neem Contact Op
          </a>
        </div>
      </section>
    </div>
  );
}
