import createMollieClient, { MollieClient } from "@mollie/api-client";

let _client: MollieClient | null = null;

function getMollieClient(): MollieClient {
  if (!_client) {
    const apiKey = process.env.MOLLIE_API_KEY;
    if (!apiKey) {
      throw new Error("MOLLIE_API_KEY is not set in environment variables");
    }
    _client = createMollieClient({ apiKey });
  }
  return _client;
}

export { getMollieClient as mollieClient };

export function getMollieClientInstance(): MollieClient {
  return getMollieClient();
}

export interface BookingPayment {
  eventSlug: string;
  eventTitle: string;
  name: string;
  email: string;
  phone: string;
  participants: number;
  amount: string;
}

export async function createPayment(booking: BookingPayment) {
  const client = getMollieClient();
  const payment = await client.payments.create({
    amount: {
      currency: "EUR",
      value: booking.amount,
    },
    description: `Boeking: ${booking.eventTitle} - ${booking.name} (${booking.participants}x)`,
    redirectUrl: `${process.env.MOLLIE_REDIRECT_URL}?event=${booking.eventSlug}`,
    webhookUrl: process.env.MOLLIE_WEBHOOK_URL,
    metadata: {
      eventSlug: booking.eventSlug,
      name: booking.name,
      email: booking.email,
      phone: booking.phone,
      participants: String(booking.participants),
    },
  });

  return payment;
}
