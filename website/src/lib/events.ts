export interface RTTEvent {
  slug: string;
  title: string;
  description: string;
  longDescription: string;
  date: string;
  time: string;
  duration: string;
  location: string;
  price: number;
  maxParticipants: number;
  spotsLeft: number;
  image: string;
  category: "training" | "event" | "competitie" | "workshop";
  features: string[];
}

// Events data - in productie kun je dit uit een CMS of database halen
export const events: RTTEvent[] = [
  {
    slug: "tactical-airsoft-training-basis",
    title: "Tactical Airsoft Training - Basis",
    description:
      "Leer de basis van tactisch bewegen, communicatie en doelgericht schieten met ons professionele BB Target System.",
    longDescription:
      "Een complete introductietraining voor beginners en gevorderden die hun tactische vaardigheden willen verbeteren. Met het Banano Pro 2 target systeem train je reflexen, precisie en tactisch inzicht in een veilige en gecontroleerde omgeving. De training omvat: veiligheidsprotocollen, wapen handling, schiethouding, bewegingstechnieken en scenario-gebaseerde oefeningen.",
    date: "2026-04-12",
    time: "10:00",
    duration: "4 uur",
    location: "RTT Training Facility",
    price: 49.95,
    maxParticipants: 16,
    spotsLeft: 8,
    image: "/images/training-basis.jpg",
    category: "training",
    features: [
      "Professionele begeleiding",
      "Banano Pro 2 target systeem",
      "Alle materialen inbegrepen",
      "Certificaat van deelname",
    ],
  },
  {
    slug: "airsoft-precision-challenge",
    title: "Airsoft Precision Challenge",
    description:
      "Test je schietvaardigheden in onze precision challenge met het geavanceerde Banano Pro 2 systeem.",
    longDescription:
      "Daag jezelf uit in verschillende schietdisciplines: snelheid, precisie en tactisch schieten. Onze Banano Pro 2 targets registreren elke hit met milliseconde-precisie. Compete tegen andere deelnemers in een spannende competitie met prijzen voor de top 3. Geschikt voor alle niveaus.",
    date: "2026-04-26",
    time: "13:00",
    duration: "3 uur",
    location: "RTT Training Facility",
    price: 34.95,
    maxParticipants: 24,
    spotsLeft: 15,
    image: "/images/precision-challenge.jpg",
    category: "competitie",
    features: [
      "Realtime scoring",
      "Meerdere disciplines",
      "Prijzen voor top 3",
      "Live leaderboard",
    ],
  },
  {
    slug: "tactical-team-experience",
    title: "Tactical Team Experience",
    description:
      "Teambuilding met een tactische twist. Perfect voor bedrijfsuitjes, vrijgezellenfeesten of vriendengroepen.",
    longDescription:
      "Een unieke teambuilding-ervaring waarbij samenwerking centraal staat. In teams van 4 doorloop je verschillende tactische scenario's met ons Banano Pro 2 target systeem. Van CQB-situaties tot precision shooting - elke ronde test andere vaardigheden. Inclusief briefing, uitrusting en een gezamenlijke evaluatie.",
    date: "2026-05-10",
    time: "11:00",
    duration: "5 uur",
    location: "RTT Training Facility",
    price: 59.95,
    maxParticipants: 20,
    spotsLeft: 12,
    image: "/images/team-experience.jpg",
    category: "event",
    features: [
      "Teambuilding activiteit",
      "Inclusief lunch",
      "Professionele begeleiding",
      "Groepsfoto & video",
    ],
  },
  {
    slug: "banano-pro-workshop",
    title: "Banano Pro 2 - Bouw Je Eigen Systeem",
    description:
      "Workshop: bouw en configureer je eigen Banano Pro 2 target systeem onder begeleiding van onze experts.",
    longDescription:
      "In deze hands-on workshop bouw je je eigen Banano Pro 2 target systeem. Je leert alles over de ESP32-technologie, sensorintegratie, LED-aansturing en het draadloze ESP-NOW protocol. Na afloop neem je een volledig werkend target systeem mee naar huis. Alle componenten zijn inbegrepen in de prijs.",
    date: "2026-05-24",
    time: "09:00",
    duration: "8 uur",
    location: "RTT Training Facility",
    price: 249.95,
    maxParticipants: 10,
    spotsLeft: 6,
    image: "/images/workshop-bouw.jpg",
    category: "workshop",
    features: [
      "Eigen target systeem meenemen",
      "Alle componenten inbegrepen",
      "Stap-voor-stap begeleiding",
      "Technische documentatie",
    ],
  },
];

export function getEvent(slug: string): RTTEvent | undefined {
  return events.find((e) => e.slug === slug);
}

export function getUpcomingEvents(): RTTEvent[] {
  const now = new Date();
  return events
    .filter((e) => new Date(e.date) >= now)
    .sort((a, b) => new Date(a.date).getTime() - new Date(b.date).getTime());
}

export function formatPrice(price: number): string {
  return new Intl.NumberFormat("nl-NL", {
    style: "currency",
    currency: "EUR",
  }).format(price);
}

export function formatDate(dateStr: string): string {
  return new Date(dateStr).toLocaleDateString("nl-NL", {
    weekday: "long",
    year: "numeric",
    month: "long",
    day: "numeric",
  });
}
