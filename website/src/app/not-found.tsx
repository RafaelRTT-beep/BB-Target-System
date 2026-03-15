import Link from "next/link";

export default function NotFound() {
  return (
    <div className="flex min-h-screen items-center justify-center pt-20">
      <div className="mx-auto max-w-lg px-4 text-center">
        <div className="font-heading text-8xl font-bold text-accent/20">404</div>
        <h1 className="mt-4 font-heading text-3xl font-bold text-white">
          Pagina niet gevonden
        </h1>
        <p className="mt-4 text-tactical-400">
          De pagina die je zoekt bestaat niet of is verplaatst.
        </p>
        <div className="mt-8 flex justify-center gap-4">
          <Link href="/" className="btn-primary">
            Naar Home
          </Link>
          <Link href="/events" className="btn-secondary">
            Bekijk Events
          </Link>
        </div>
      </div>
    </div>
  );
}
