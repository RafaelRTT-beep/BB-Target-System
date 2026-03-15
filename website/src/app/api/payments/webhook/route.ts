import { NextRequest, NextResponse } from "next/server";
import { getMollieClientInstance } from "@/lib/mollie";

export async function POST(request: NextRequest) {
  try {
    const body = await request.formData();
    const paymentId = body.get("id") as string;

    if (!paymentId) {
      return NextResponse.json(
        { error: "Payment ID is required" },
        { status: 400 }
      );
    }

    const mollieClient = getMollieClientInstance();
    const payment = await mollieClient.payments.get(paymentId);

    if (payment.isPaid()) {
      // Payment successful - process the booking
      const metadata = payment.metadata as {
        eventSlug: string;
        name: string;
        email: string;
        phone: string;
        participants: string;
      };

      console.log("Payment successful:", {
        paymentId: payment.id,
        amount: payment.amount,
        event: metadata.eventSlug,
        name: metadata.name,
        email: metadata.email,
        participants: metadata.participants,
      });

      // TODO: Save booking to database
      // TODO: Send confirmation email to customer
      // TODO: Update available spots
    } else if (payment.isFailed() || payment.isExpired() || payment.isCanceled()) {
      console.log("Payment not completed:", {
        paymentId: payment.id,
        status: payment.status,
      });
    }

    // Always return 200 to Mollie
    return NextResponse.json({ received: true });
  } catch (error) {
    console.error("Webhook error:", error);
    // Still return 200 to prevent Mollie from retrying
    return NextResponse.json({ received: true });
  }
}
