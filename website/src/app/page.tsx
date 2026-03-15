import Link from "next/link";
import { getUpcomingEvents, formatPrice, formatDate } from "@/lib/events";

function HeroSection() {
  return (
    <section className="relative flex min-h-screen items-center overflow-hidden pt-20">
      {/* Background gradient */}
      <div className="absolute inset-0 bg-gradient-to-br from-tactical-950 via-tactical-900 to-tactical-950" />
      <div className="absolute inset-0 bg-[radial-gradient(ellipse_at_top_right,_var(--tw-gradient-stops))] from-accent/10 via-transparent to-transparent" />
      <div className="absolute inset-0 bg-[radial-gradient(ellipse_at_bottom_left,_var(--tw-gradient-stops))] from-primary-500/10 via-transparent to-transparent" />

      {/* Grid pattern overlay */}
      <div
        className="absolute inset-0 opacity-5"
        style={{
          backgroundImage:
            "linear-gradient(rgba(255,255,255,0.1) 1px, transparent 1px), linear-gradient(90deg, rgba(255,255,255,0.1) 1px, transparent 1px)",
          backgroundSize: "50px 50px",
        }}
      />

      <div className="relative mx-auto max-w-7xl px-4 py-20 sm:px-6 lg:px-8">
        <div className="max-w-3xl">
          <div className="mb-6 inline-flex items-center rounded-full border border-primary-500/30 bg-primary-500/10 px-4 py-1.5 text-sm text-primary-400">
            <span className="mr-2 h-2 w-2 rounded-full bg-primary-400" />
            1000+ m&sup2; Indoor Tactical Arena - Den Haag
          </div>

          <h1 className="font-heading text-5xl font-bold leading-tight tracking-tight sm:text-6xl lg:text-7xl">
            <span className="text-white">De Ultieme</span>
            <br />
            <span className="gradient-text">Airsoft Ervaring</span>
          </h1>

          <p className="mt-6 max-w-xl text-lg text-tactical-300 sm:text-xl">
            Professionele CQB airsoft trainingen en events in Den Haag.
            Begeleid door ex-Defensie en politie instructeurs. Nu met het
            revolutionaire{" "}
            <span className="font-semibold text-accent">Banano Pro 2</span>{" "}
            target systeem.
          </p>

          <div className="mt-10 flex flex-col gap-4 sm:flex-row">
            <Link href="/events" className="btn-primary text-lg">
              Bekijk Events & Trainingen
            </Link>
            <Link href="/#banano-pro" className="btn-secondary text-lg">
              Ontdek Banano Pro 2
            </Link>
          </div>

          {/* Stats */}
          <div className="mt-16 grid grid-cols-2 gap-8 sm:grid-cols-4">
            {[
              { value: "1000+", label: "m\u00B2 Arena" },
              { value: "8", label: "Target Zones" },
              { value: "<5ms", label: "Hit Detectie" },
              { value: "4", label: "Game Modes" },
            ].map((stat) => (
              <div key={stat.label}>
                <div className="font-heading text-3xl font-bold text-accent">
                  {stat.value}
                </div>
                <div className="mt-1 text-sm text-tactical-400">
                  {stat.label}
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </section>
  );
}

function AboutSection() {
  return (
    <section id="over-ons" className="py-24">
      <div className="mx-auto max-w-7xl px-4 sm:px-6 lg:px-8">
        <div className="grid items-center gap-16 lg:grid-cols-2">
          <div>
            <h2 className="section-heading">
              Waarom <span className="gradient-text">Running The Target</span>?
            </h2>
            <p className="section-subheading">
              Al jaren de meest professionele indoor airsoft locatie van Nederland.
              Opgericht door specialisten met een achtergrond bij Defensie en politie.
            </p>

            <div className="mt-10 space-y-6">
              {[
                {
                  title: "Professionele Instructeurs",
                  desc: "Begeleiding door ex-militairen en politie-specialisten die je veilig en effectief trainen.",
                },
                {
                  title: "Modulaire Tactische Hal",
                  desc: "1000+ m\u00B2 met ingerichte kamers, gangen, licht- en geluidseffecten voor realistische scenario's.",
                },
                {
                  title: "Alle Niveaus Welkom",
                  desc: "Van complete beginner tot gevorderde speler - groepen worden altijd ingedeeld op niveau.",
                },
                {
                  title: "Complete Faciliteiten",
                  desc: "Kantine met bar, kleedkamers, douches en een aparte shooting range.",
                },
              ].map((item) => (
                <div key={item.title} className="flex gap-4">
                  <div className="mt-1 flex h-8 w-8 shrink-0 items-center justify-center rounded-lg bg-accent/10">
                    <svg
                      className="h-4 w-4 text-accent"
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
                  </div>
                  <div>
                    <h3 className="font-heading text-lg font-semibold text-white">
                      {item.title}
                    </h3>
                    <p className="mt-1 text-tactical-400">{item.desc}</p>
                  </div>
                </div>
              ))}
            </div>
          </div>

          {/* Placeholder for image / visual */}
          <div className="relative">
            <div className="aspect-square overflow-hidden rounded-2xl border border-tactical-800 bg-gradient-to-br from-tactical-800 to-tactical-900">
              <div className="flex h-full flex-col items-center justify-center p-8 text-center">
                <div className="font-heading text-6xl font-bold text-accent/20">
                  RTT
                </div>
                <p className="mt-4 text-tactical-500">
                  Tactical Airsoft Simulations
                </p>
                <p className="mt-2 text-sm text-tactical-600">
                  Voeg hier je eigen afbeelding toe
                </p>
              </div>
            </div>
            {/* Decorative elements */}
            <div className="absolute -right-4 -top-4 h-24 w-24 rounded-2xl border border-accent/20 bg-accent/5" />
            <div className="absolute -bottom-4 -left-4 h-32 w-32 rounded-2xl border border-primary-500/20 bg-primary-500/5" />
          </div>
        </div>
      </div>
    </section>
  );
}

function BananoProSection() {
  return (
    <section
      id="banano-pro"
      className="relative overflow-hidden bg-tactical-900/50 py-24"
    >
      <div className="absolute inset-0 bg-[radial-gradient(ellipse_at_center,_var(--tw-gradient-stops))] from-accent/5 via-transparent to-transparent" />

      <div className="relative mx-auto max-w-7xl px-4 sm:px-6 lg:px-8">
        <div className="text-center">
          <div className="mb-4 inline-flex items-center rounded-full border border-accent/30 bg-accent/10 px-4 py-1.5 text-sm text-accent">
            Nieuw &mdash; Exclusief bij RTT
          </div>
          <h2 className="section-heading">
            Het <span className="gradient-text">Banano Pro 2</span> Target
            Systeem
          </h2>
          <p className="section-subheading mx-auto">
            Het meest geavanceerde draadloze target systeem voor airsoft training.
            Ontwikkeld door RTT voor de ultieme trainings-ervaring.
          </p>
        </div>

        <div className="mt-16 grid gap-8 sm:grid-cols-2 lg:grid-cols-4">
          {[
            {
              icon: (
                <svg className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M13 10V3L4 14h7v7l9-11h-7z" />
                </svg>
              ),
              title: "Razendsnelle Detectie",
              desc: "< 5ms hit-detectie via ESP-NOW protocol. Sneller dan je kunt knipperen.",
            },
            {
              icon: (
                <svg className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 19V6l12-3v13M9 19c0 1.105-1.343 2-3 2s-3-.895-3-2 1.343-2 3-2 3 .895 3 2zm12-3c0 1.105-1.343 2-3 2s-3-.895-3-2 1.343-2 3-2 3 .895 3 2zM9 10l12-3" />
                </svg>
              ),
              title: "8 Draadloze Targets",
              desc: "Volledig draadloos systeem met 8 onafhankelijke targets. Geen kabels, geen gedoe.",
            },
            {
              icon: (
                <svg className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9.75 17L9 20l-1 1h8l-1-1-.75-3M3 13h18M5 17h14a2 2 0 002-2V5a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z" />
                </svg>
              ),
              title: "Realtime Dashboard",
              desc: "Live scores, timer en leaderboard op elk apparaat. Perfect voor competities.",
            },
            {
              icon: (
                <svg className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z" />
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                </svg>
              ),
              title: "4 Game Modes",
              desc: "Free Play, Sequence, Random en Shoot/No Shoot. Eindeloze variatie in training.",
            },
          ].map((feature) => (
            <div key={feature.title} className="card text-center">
              <div className="mx-auto flex h-12 w-12 items-center justify-center rounded-xl bg-accent/10 text-accent">
                {feature.icon}
              </div>
              <h3 className="mt-4 font-heading text-lg font-semibold text-white">
                {feature.title}
              </h3>
              <p className="mt-2 text-sm text-tactical-400">{feature.desc}</p>
            </div>
          ))}
        </div>

        {/* CTA */}
        <div className="mt-16 text-center">
          <Link
            href="/events/banano-pro-workshop"
            className="btn-primary text-lg"
          >
            Bouw Je Eigen Systeem - Workshop
          </Link>
          <p className="mt-3 text-sm text-tactical-500">
            Of ervaar het zelf tijdens een van onze trainingen
          </p>
        </div>
      </div>
    </section>
  );
}

function FacilitySection() {
  return (
    <section id="faciliteit" className="py-24">
      <div className="mx-auto max-w-7xl px-4 sm:px-6 lg:px-8">
        <div className="text-center">
          <h2 className="section-heading">
            Onze <span className="gradient-text">Faciliteit</span>
          </h2>
          <p className="section-subheading mx-auto">
            Een professioneel ingerichte tactische arena in het hart van Den Haag.
          </p>
        </div>

        <div className="mt-16 grid gap-6 sm:grid-cols-2 lg:grid-cols-3">
          {[
            {
              title: "Tactische Arena",
              desc: "1000+ m\u00B2 met modulaire wanden, ingerichte kamers, gangen en open ruimtes. Volledig aanpasbaar per scenario.",
              icon: "M3 12l2-2m0 0l7-7 7 7M5 10v10a1 1 0 001 1h3m10-11l2 2m-2-2v10a1 1 0 01-1 1h-3m-6 0a1 1 0 001-1v-4a1 1 0 011-1h2a1 1 0 011 1v4a1 1 0 001 1m-6 0h6",
            },
            {
              title: "Shooting Range",
              desc: "Aparte schietbaan met het Banano Pro 2 systeem voor precision training en warming up.",
              icon: "M13 10V3L4 14h7v7l9-11h-7z",
            },
            {
              title: "Licht & Geluid",
              desc: "Professionele licht- en geluidsinstallatie voor realistische scenario's en sfeer.",
              icon: "M9.663 17h4.673M12 3v1m6.364 1.636l-.707.707M21 12h-1M4 12H3m3.343-5.657l-.707-.707m2.828 9.9a5 5 0 117.072 0l-.548.547A3.374 3.374 0 0014 18.469V19a2 2 0 11-4 0v-.531c0-.895-.356-1.754-.988-2.386l-.548-.547z",
            },
            {
              title: "Kantine & Bar",
              desc: "Ontspan na de actie met een drankje in onze kantine. Snacks en dranken beschikbaar.",
              icon: "M5 3v4M3 5h4M6 17v4m-2-2h4m5-16l2.286 6.857L21 12l-5.714 2.143L13 21l-2.286-6.857L5 12l5.714-2.143L13 3z",
            },
            {
              title: "Kleedkamers",
              desc: "Ruime kleedkamers met lockers en douchefaciliteiten voor na de training.",
              icon: "M16 7a4 4 0 11-8 0 4 4 0 018 0zM12 14a7 7 0 00-7 7h14a7 7 0 00-7-7z",
            },
            {
              title: "Veiligheid",
              desc: "Uitgebreide veiligheidsprotocollen. Joule-check, verplichte face protection en ervaren marshalls.",
              icon: "M9 12l2 2 4-4m5.618-4.016A11.955 11.955 0 0112 2.944a11.955 11.955 0 01-8.618 3.04A12.02 12.02 0 003 9c0 5.591 3.824 10.29 9 11.622 5.176-1.332 9-6.03 9-11.622 0-1.042-.133-2.052-.382-3.016z",
            },
          ].map((item) => (
            <div key={item.title} className="card">
              <div className="flex h-10 w-10 items-center justify-center rounded-lg bg-primary-500/10">
                <svg
                  className="h-5 w-5 text-primary-400"
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
              </div>
              <h3 className="mt-4 font-heading text-lg font-semibold text-white">
                {item.title}
              </h3>
              <p className="mt-2 text-sm text-tactical-400">{item.desc}</p>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}

function EventsPreviewSection() {
  const upcomingEvents = getUpcomingEvents().slice(0, 3);

  return (
    <section className="bg-tactical-900/50 py-24">
      <div className="mx-auto max-w-7xl px-4 sm:px-6 lg:px-8">
        <div className="flex items-end justify-between">
          <div>
            <h2 className="section-heading">
              Aankomende <span className="gradient-text">Events</span>
            </h2>
            <p className="section-subheading">
              Schrijf je in voor onze trainingen, competities en workshops.
            </p>
          </div>
          <Link href="/events" className="btn-ghost hidden sm:inline-flex">
            Alle events bekijken &rarr;
          </Link>
        </div>

        <div className="mt-12 grid gap-8 sm:grid-cols-2 lg:grid-cols-3">
          {upcomingEvents.map((event) => (
            <Link
              key={event.slug}
              href={`/events/${event.slug}`}
              className="card group"
            >
              {/* Category badge */}
              <div className="mb-4 inline-flex rounded-full bg-accent/10 px-3 py-1 text-xs font-medium capitalize text-accent">
                {event.category}
              </div>

              <h3 className="font-heading text-xl font-semibold text-white transition-colors group-hover:text-accent">
                {event.title}
              </h3>
              <p className="mt-2 line-clamp-2 text-sm text-tactical-400">
                {event.description}
              </p>

              <div className="mt-6 flex items-center justify-between border-t border-tactical-800 pt-4">
                <div>
                  <div className="text-sm font-medium text-white">
                    {formatDate(event.date)}
                  </div>
                  <div className="text-xs text-tactical-500">
                    {event.time} - {event.duration}
                  </div>
                </div>
                <div className="text-right">
                  <div className="text-lg font-bold text-accent">
                    {formatPrice(event.price)}
                  </div>
                  <div className="text-xs text-tactical-500">
                    {event.spotsLeft} plekken over
                  </div>
                </div>
              </div>
            </Link>
          ))}
        </div>

        <div className="mt-8 text-center sm:hidden">
          <Link href="/events" className="btn-ghost">
            Alle events bekijken &rarr;
          </Link>
        </div>
      </div>
    </section>
  );
}

function GameModesSection() {
  return (
    <section className="py-24">
      <div className="mx-auto max-w-7xl px-4 sm:px-6 lg:px-8">
        <div className="text-center">
          <h2 className="section-heading">
            Airsoft <span className="gradient-text">Game Modes</span>
          </h2>
          <p className="section-subheading mx-auto">
            Van team vs team tot precision challenges - bij RTT is er voor
            iedereen wat wils.
          </p>
        </div>

        <div className="mt-16 grid gap-6 sm:grid-cols-2">
          {[
            {
              title: "Defuse the Bomb",
              desc: "Team-gebaseerd scenario waarbij het aanvallende team de bom moet plaatsen en verdedigen. Tactisch samenwerken is de sleutel.",
              color: "from-red-500/10 to-transparent",
              border: "border-red-500/20",
            },
            {
              title: "Domination",
              desc: "Verover en houd strategische punten vast. Punten worden berekend op basis van bezettingstijd. Elke seconde telt.",
              color: "from-blue-500/10 to-transparent",
              border: "border-blue-500/20",
            },
            {
              title: "Knock Knock",
              desc: "Intensieve door-kicking CQB actie. Kamers clearen, hoeken checken en communicatie met je team.",
              color: "from-amber-500/10 to-transparent",
              border: "border-amber-500/20",
            },
            {
              title: "Precision Challenge",
              desc: "Individuele vaardigheidstest met het Banano Pro 2 systeem. Snelheid, precisie en tactisch inzicht worden gemeten.",
              color: "from-green-500/10 to-transparent",
              border: "border-green-500/20",
            },
          ].map((mode) => (
            <div
              key={mode.title}
              className={`rounded-2xl border ${mode.border} bg-gradient-to-br ${mode.color} p-8 transition-all duration-300 hover:scale-[1.02]`}
            >
              <h3 className="font-heading text-2xl font-bold text-white">
                {mode.title}
              </h3>
              <p className="mt-3 text-tactical-300">{mode.desc}</p>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}

function CTASection() {
  return (
    <section id="contact" className="py-24">
      <div className="mx-auto max-w-7xl px-4 sm:px-6 lg:px-8">
        <div className="relative overflow-hidden rounded-3xl border border-tactical-800 bg-gradient-to-br from-tactical-900 via-tactical-800 to-tactical-900 p-12 text-center sm:p-16">
          <div className="absolute inset-0 bg-[radial-gradient(ellipse_at_top,_var(--tw-gradient-stops))] from-accent/10 via-transparent to-transparent" />

          <div className="relative">
            <h2 className="font-heading text-3xl font-bold text-white sm:text-4xl lg:text-5xl">
              Klaar voor de <span className="gradient-text">actie</span>?
            </h2>
            <p className="mx-auto mt-4 max-w-xl text-lg text-tactical-300">
              Boek een training, schrijf je in voor een event of neem contact op
              voor een teambuilding op maat.
            </p>

            <div className="mt-10 flex flex-col items-center gap-4 sm:flex-row sm:justify-center">
              <Link href="/events" className="btn-primary text-lg">
                Bekijk alle Events
              </Link>
              <a href="tel:+31641102662" className="btn-secondary text-lg">
                Bel: +31 6 41102662
              </a>
            </div>

            <div className="mt-10 grid gap-8 sm:grid-cols-3">
              {[
                { label: "Email", value: "info@rttfuture.nl", href: "mailto:info@rttfuture.nl" },
                { label: "Telefoon", value: "+31 6 41102662", href: "tel:+31641102662" },
                { label: "Locatie", value: "Den Haag, Zuid-Holland", href: "#" },
              ].map((item) => (
                <a
                  key={item.label}
                  href={item.href}
                  className="rounded-xl bg-tactical-950/50 p-4 transition-colors hover:bg-tactical-950"
                >
                  <div className="text-sm text-tactical-500">{item.label}</div>
                  <div className="mt-1 font-medium text-white">
                    {item.value}
                  </div>
                </a>
              ))}
            </div>
          </div>
        </div>
      </div>
    </section>
  );
}

export default function Home() {
  return (
    <>
      <HeroSection />
      <AboutSection />
      <BananoProSection />
      <FacilitySection />
      <GameModesSection />
      <EventsPreviewSection />
      <CTASection />
    </>
  );
}
