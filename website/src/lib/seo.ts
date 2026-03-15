import type { Metadata } from "next";

const SITE_URL = process.env.NEXT_PUBLIC_SITE_URL || "https://rttfuture.nl";
const SITE_NAME = process.env.NEXT_PUBLIC_SITE_NAME || "Running The Target";

export function createMetadata(options: {
  title?: string;
  description: string;
  path?: string;
  image?: string;
  type?: "website" | "article";
  keywords?: string[];
}): Metadata {
  const title = options.title
    ? `${options.title} | ${SITE_NAME}`
    : `${SITE_NAME} - Professionele Airsoft & Tactische Training`;

  const baseKeywords = [
    "airsoft",
    "tactische training",
    "airsoft training nederland",
    "tactical training",
    "schiettraining",
    "airsoft events",
    "airsoft competitie",
    "BB target systeem",
    "Banano Pro 2",
    "Running The Target",
    "RTT Future",
    "airsoft teambuilding",
    "tactical shooting",
    "airsoft nederland",
    "CQB training",
    "precision shooting",
    "airsoft workshop",
    "schietdoel systeem",
    "ESP32 target system",
    "draadloos target systeem",
  ];

  const keywords = [...baseKeywords, ...(options.keywords || [])];

  return {
    title,
    description: options.description,
    keywords: keywords.join(", "),
    authors: [{ name: SITE_NAME }],
    creator: SITE_NAME,
    publisher: SITE_NAME,
    metadataBase: new URL(SITE_URL),
    alternates: {
      canonical: options.path ? `${SITE_URL}${options.path}` : SITE_URL,
    },
    openGraph: {
      title,
      description: options.description,
      url: options.path ? `${SITE_URL}${options.path}` : SITE_URL,
      siteName: SITE_NAME,
      locale: "nl_NL",
      type: options.type || "website",
      images: options.image
        ? [{ url: options.image, width: 1200, height: 630, alt: title }]
        : undefined,
    },
    twitter: {
      card: "summary_large_image",
      title,
      description: options.description,
    },
    robots: {
      index: true,
      follow: true,
      googleBot: {
        index: true,
        follow: true,
        "max-video-preview": -1,
        "max-image-preview": "large",
        "max-snippet": -1,
      },
    },
  };
}

export function generateEventStructuredData(event: {
  title: string;
  description: string;
  date: string;
  time: string;
  duration: string;
  location: string;
  price: number;
  spotsLeft: number;
  slug: string;
}) {
  return {
    "@context": "https://schema.org",
    "@type": "Event",
    name: event.title,
    description: event.description,
    startDate: `${event.date}T${event.time}:00+02:00`,
    location: {
      "@type": "Place",
      name: event.location,
      address: {
        "@type": "PostalAddress",
        addressCountry: "NL",
      },
    },
    offers: {
      "@type": "Offer",
      price: event.price.toFixed(2),
      priceCurrency: "EUR",
      availability:
        event.spotsLeft > 0
          ? "https://schema.org/InStock"
          : "https://schema.org/SoldOut",
      url: `${SITE_URL}/events/${event.slug}`,
    },
    organizer: {
      "@type": "Organization",
      name: SITE_NAME,
      url: SITE_URL,
    },
  };
}

export function generateOrganizationStructuredData() {
  return {
    "@context": "https://schema.org",
    "@type": "Organization",
    name: SITE_NAME,
    url: SITE_URL,
    description:
      "Professionele airsoft en tactische training met het Banano Pro 2 target systeem",
    address: {
      "@type": "PostalAddress",
      addressCountry: "NL",
    },
    sameAs: [],
  };
}

export function generateLocalBusinessStructuredData() {
  return {
    "@context": "https://schema.org",
    "@type": "LocalBusiness",
    name: SITE_NAME,
    url: SITE_URL,
    description:
      "Professionele airsoft en tactische trainingen, events en workshops met innovatieve target systemen",
    address: {
      "@type": "PostalAddress",
      addressCountry: "NL",
    },
    priceRange: "€€",
    openingHours: "Mo-Su 09:00-21:00",
  };
}
