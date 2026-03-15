import { notFound } from "next/navigation";
import Link from "next/link";
import { getEvent, events, formatPrice, formatDate } from "@/lib/events";
import { createMetadata, generateEventStructuredData } from "@/lib/seo";
import { BookingForm } from "@/components/BookingForm";

type Props = {
  params: { slug: string };
};

export async function generateStaticParams() {
  return events.map((event) => ({ slug: event.slug }));
}

export async function generateMetadata({ params }: Props) {
  const event = getEvent(params.slug);
  if (!event) return {};

  return createMetadata({
    title: event.title,
    description: event.description,
    path: `/events/${event.slug}`,
    type: "article",
    keywords: [
      event.title.toLowerCase(),
      event.category,
      `${event.category} den haag`,
      "airsoft event boeken",
    ],
  });
}

export default function EventPage({ params }: Props) {
  const event = getEvent(params.slug);

  if (!event) {
    notFound();
  }

  const structuredData = generateEventStructuredData(event);

  return (
    <>
      <script
        type="application/ld+json"
        dangerouslySetInnerHTML={{ __html: JSON.stringify(structuredData) }}
      />

      <div className="pt-24">
        {/* Breadcrumb */}
        <div className="mx-auto max-w-7xl px-4 py-4 sm:px-6 lg:px-8">
          <nav className="flex items-center gap-2 text-sm text-tactical-500">
            <Link href="/" className="hover:text-tactical-300">
              Home
            </Link>
            <span>/</span>
            <Link href="/events" className="hover:text-tactical-300">
              Events
            </Link>
            <span>/</span>
            <span className="text-tactical-300">{event.title}</span>
          </nav>
        </div>

        <div className="mx-auto max-w-7xl px-4 py-8 sm:px-6 lg:px-8">
          <div className="grid gap-12 lg:grid-cols-3">
            {/* Event Details */}
            <div className="lg:col-span-2">
              <div className="inline-flex rounded-full bg-accent/10 px-3 py-1 text-sm font-medium capitalize text-accent">
                {event.category}
              </div>

              <h1 className="mt-4 font-heading text-4xl font-bold text-white sm:text-5xl">
                {event.title}
              </h1>

              <p className="mt-6 text-lg leading-relaxed text-tactical-300">
                {event.longDescription}
              </p>

              {/* Features */}
              <div className="mt-10">
                <h2 className="font-heading text-xl font-semibold text-white">
                  Wat krijg je?
                </h2>
                <ul className="mt-4 space-y-3">
                  {event.features.map((feature) => (
                    <li key={feature} className="flex items-center gap-3">
                      <svg
                        className="h-5 w-5 shrink-0 text-primary-400"
                        fill="none"
                        viewBox="0 0 24 24"
                        stroke="currentColor"
                      >
                        <path
                          strokeLinecap="round"
                          strokeLinejoin="round"
                          strokeWidth={2}
                          d="M5 13l4 4L19 7"
                        />
                      </svg>
                      <span className="text-tactical-300">{feature}</span>
                    </li>
                  ))}
                </ul>
              </div>

              {/* Event Info Grid */}
              <div className="mt-10 grid gap-4 sm:grid-cols-2">
                {[
                  {
                    icon: "M8 7V3m8 4V3m-9 8h10M5 21h14a2 2 0 002-2V7a2 2 0 00-2-2H5a2 2 0 00-2 2v12a2 2 0 002 2z",
                    label: "Datum",
                    value: formatDate(event.date),
                  },
                  {
                    icon: "M12 8v4l3 3m6-3a9 9 0 11-18 0 9 9 0 0118 0z",
                    label: "Tijd & Duur",
                    value: `${event.time} - ${event.duration}`,
                  },
                  {
                    icon: "M17.657 16.657L13.414 20.9a1.998 1.998 0 01-2.827 0l-4.244-4.243a8 8 0 1111.314 0z",
                    label: "Locatie",
                    value: event.location,
                  },
                  {
                    icon: "M17 20h5v-2a3 3 0 00-5.356-1.857M17 20H7m10 0v-2c0-.656-.126-1.283-.356-1.857M7 20H2v-2a3 3 0 015.356-1.857M7 20v-2c0-.656.126-1.283.356-1.857m0 0a5.002 5.002 0 019.288 0M15 7a3 3 0 11-6 0 3 3 0 016 0z",
                    label: "Beschikbaar",
                    value: `${event.spotsLeft} van ${event.maxParticipants} plekken`,
                  },
                ].map((item) => (
                  <div
                    key={item.label}
                    className="flex items-start gap-3 rounded-xl border border-tactical-800 bg-tactical-900/50 p-4"
                  >
                    <svg
                      className="h-5 w-5 shrink-0 text-accent"
                      fill="none"
                      viewBox="0 0 24 24"
                      stroke="currentColor"
                    >
                      <path
                        strokeLinecap="round"
                        strokeLinejoin="round"
                        strokeWidth={2}
                        d={item.icon}
                      />
                    </svg>
                    <div>
                      <div className="text-xs text-tactical-500">
                        {item.label}
                      </div>
                      <div className="mt-0.5 font-medium text-white">
                        {item.value}
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </div>

            {/* Booking Sidebar */}
            <div className="lg:col-span-1">
              <div className="sticky top-24 rounded-2xl border border-tactical-800 bg-tactical-900/80 p-6 backdrop-blur-sm">
                <div className="text-center">
                  <div className="text-3xl font-bold text-accent">
                    {formatPrice(event.price)}
                  </div>
                  <div className="text-sm text-tactical-500">per persoon</div>
                </div>

                {event.spotsLeft > 0 ? (
                  <BookingForm
                    eventSlug={event.slug}
                    eventTitle={event.title}
                    price={event.price}
                  />
                ) : (
                  <div className="mt-6 rounded-xl bg-red-500/10 p-4 text-center">
                    <p className="font-semibold text-red-400">Uitverkocht</p>
                    <p className="mt-1 text-sm text-tactical-400">
                      Dit event is helaas vol. Neem contact op voor de
                      wachtlijst.
                    </p>
                  </div>
                )}

                {event.spotsLeft > 0 && event.spotsLeft <= 5 && (
                  <p className="mt-4 text-center text-sm text-red-400">
                    Nog maar {event.spotsLeft} plekken beschikbaar!
                  </p>
                )}
              </div>
            </div>
          </div>
        </div>
      </div>
    </>
  );
}
