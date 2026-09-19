import { Card, CardContent } from '@/components/ui/card';
import { AlertCircle } from 'lucide-react';
import { Link } from 'wouter';

export default function NotFound() {
  return (
    <div className="flex min-h-[100dvh] w-full items-center justify-center bg-[#eee9df] p-5">
      <Card className="w-full max-w-md border-[#d9d3c7] bg-[#f8f6f0] shadow-[8px_8px_0_#102b3b]">
        <CardContent className="pt-6">
          <div className="mb-4 flex gap-3">
            <AlertCircle className="h-8 w-8 text-[#ef6c52]" />
            <h1 className="font-[var(--app-font-serif)] text-2xl font-bold text-[#163243]">
              Route not found
            </h1>
          </div>

          <p className="mt-4 text-sm leading-6 text-[#6d7e79]">
            Guardian could not locate that workspace view.
          </p>
          <Link href="/" className="mt-6 inline-flex bg-[#f2c94c] px-4 py-2 font-mono text-[10px] font-bold uppercase tracking-[0.14em] text-[#102b3b]" data-testid="link-return-posture">Return to posture</Link>
        </CardContent>
      </Card>
    </div>
  );
}
