import { MetadataRoute } from "next";
import { events } from "@/lib/events";

const SITE_URL = process.env.NEXT_PUBLIC_SITE_URL || "https://rttfuture.nl";

export default function sitemap(): MetadataRoute.Sitemap {
  const eventPages = events.map((event) => ({
    url: `${SITE_URL}/events/${event.slug}`,
    lastModified: new Date(),
    changeFrequency: "weekly" as const,
    priority: 0.8,
  }));

  return [
    {
      url: SITE_URL,
      lastModified: new Date(),
      changeFrequency: "weekly",
      priority: 1,
    },
    {
      url: `${SITE_URL}/events`,
      lastModified: new Date(),
      changeFrequency: "daily",
      priority: 0.9,
    },
    ...eventPages,
  ];
}
