import type { Metadata } from "next";
import localFont from "next/font/local";
import "./globals.css";
import { Header } from "@/components/Header";
import { Footer } from "@/components/Footer";
import {
  createMetadata,
  generateOrganizationStructuredData,
  generateLocalBusinessStructuredData,
} from "@/lib/seo";

export const metadata: Metadata = createMetadata({
  description:
    "Running The Target - De ultieme indoor CQB airsoft ervaring in Den Haag. Professionele tactische trainingen, events en competities met het innovatieve Banano Pro 2 target systeem. 1000+ m\u00B2 tactische hal.",
  keywords: [
    "airsoft den haag",
    "indoor airsoft",
    "CQB airsoft nederland",
    "tactische training den haag",
    "airsoft teambuilding",
    "schiettraining den haag",
  ],
});

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="nl">
      <head>
        <script
          type="application/ld+json"
          dangerouslySetInnerHTML={{
            __html: JSON.stringify(generateOrganizationStructuredData()),
          }}
        />
        <script
          type="application/ld+json"
          dangerouslySetInnerHTML={{
            __html: JSON.stringify(generateLocalBusinessStructuredData()),
          }}
        />
      </head>
      <body className="font-body">
        <Header />
        <main>{children}</main>
        <Footer />
      </body>
    </html>
  );
}
