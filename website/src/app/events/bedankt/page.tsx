import Link from "next/link";
import { createMetadata } from "@/lib/seo";

export const metadata = createMetadata({
  title: "Bedankt voor je boeking",
  description: "Je boeking bij Running The Target is bevestigd.",
  path: "/events/bedankt",
});

export default function BedanktPage() {
  return (
    <div className="flex min-h-screen items-center justify-center pt-20">
      <div className="mx-auto max-w-lg px-4 text-center">
        <div className="mx-auto flex h-20 w-20 items-center justify-center rounded-full bg-primary-500/10">
          <svg
            className="h-10 w-10 text-primary-400"
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

        <h1 className="mt-6 font-heading text-3xl font-bold text-white">
          Bedankt voor je boeking!
        </h1>
        <p className="mt-4 text-tactical-300">
          Je betaling is ontvangen. Je ontvangt een bevestigingsmail met alle
          details over je boeking. We zien je graag bij Running The Target!
        </p>

        <div className="mt-8 flex flex-col gap-4 sm:flex-row sm:justify-center">
          <Link href="/events" className="btn-primary">
            Meer Events Bekijken
          </Link>
          <Link href="/" className="btn-ghost">
            Terug naar Home
          </Link>
        </div>
      </div>
    </div>
  );
}
