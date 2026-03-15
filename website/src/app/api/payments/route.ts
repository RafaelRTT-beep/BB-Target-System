import { NextRequest, NextResponse } from "next/server";
import { createPayment, type BookingPayment } from "@/lib/mollie";

export async function POST(request: NextRequest) {
  try {
    const body = await request.json();

    // Validate required fields
    const { eventSlug, eventTitle, name, email, phone, participants, amount } =
      body;

    if (
      !eventSlug ||
      !eventTitle ||
      !name ||
      !email ||
      !phone ||
      !participants ||
      !amount
    ) {
      return NextResponse.json(
        { error: "Alle velden zijn verplicht." },
        { status: 400 }
      );
    }

    // Basic email validation
    const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    if (!emailRegex.test(email)) {
      return NextResponse.json(
        { error: "Ongeldig e-mailadres." },
        { status: 400 }
      );
    }

    // Validate participants
    if (participants < 1 || participants > 8) {
      return NextResponse.json(
        { error: "Aantal deelnemers moet tussen 1 en 8 zijn." },
        { status: 400 }
      );
    }

    const bookingData: BookingPayment = {
      eventSlug,
      eventTitle,
      name: String(name).trim(),
      email: String(email).trim().toLowerCase(),
      phone: String(phone).trim(),
      participants: Number(participants),
      amount: String(amount),
    };

    const payment = await createPayment(bookingData);

    return NextResponse.json({
      paymentId: payment.id,
      checkoutUrl: payment.getCheckoutUrl(),
    });
  } catch (error) {
    console.error("Payment creation error:", error);
    return NextResponse.json(
      { error: "Er ging iets mis bij het aanmaken van de betaling. Probeer het later opnieuw." },
      { status: 500 }
    );
  }
}
