import { useState } from "react";
import { Check, Clipboard, Cpu, Download, FileCode2, Play, ShieldCheck, Terminal } from "lucide-react";

const cx = (...classes: Array<string | false | null | undefined>) => classes.filter(Boolean).join(" ");

function CopyButton({ value }: { value: string }) {
  const [copied, setCopied] = useState(false);
  const copy = async () => {
    await navigator.clipboard.writeText(value);
    setCopied(true);
    window.setTimeout(() => setCopied(false), 1600);
  };
  return (
    <button onClick={copy} className="inline-flex items-center gap-2 border border-[#48616b] px-3 py-2 font-mono text-[10px] uppercase tracking-[0.14em] text-[#d7e7df] transition hover:border-[#f2c94c] hover:text-[#f2c94c]">
      {copied ? <Check size={14} /> : <Clipboard size={14} />}
      {copied ? "Copied" : "Copy"}
    </button>
  );
}

function CodeBlock({ label, value }: { label: string; value: string }) {
  return (
    <div className="overflow-hidden border border-[#355361] bg-[#102b3b] shadow-[5px_5px_0_#d6cfbf]">
      <div className="flex items-center justify-between border-b border-[#355361] px-4 py-3">
        <div className="flex items-center gap-2 font-mono text-[10px] uppercase tracking-[0.18em] text-[#8db1ae]"><Terminal size={14} /> {label}</div>
        <CopyButton value={value} />
      </div>
      <pre className="overflow-x-auto px-4 py-4 font-mono text-xs leading-6 text-[#e7eee5]"><code>{value}</code></pre>
    </div>
  );
}

const downloadCommand = `curl -fsSL https://raw.githubusercontent.com/fadalstore/Fadal-watch/main/scripts/build-test-fadalwatch.sh \\
  | bash -s -- --source-dir "$HOME/FadalWatch-source" --no-uefi`;
const buildCommand = `cd "$HOME/FadalWatch-source"\nmake -C kernel CC=clang`;
const testCommand = `cd "$HOME/FadalWatch-source"\nmake -C kernel CC=clang QEMU=qemu-system-i386 check`;
const shellCommands = `help\nls\ncat KERNEL.TXT\nstat KERNEL.TXT\nfscan\nexit`;

const pipeline = [
  { icon: Download, number: "01", title: "Soo dejiso", detail: "Script-ku wuxuu soo qaataa source-ka branch-ka main oo ku rakibaa folder cusub." },
  { icon: Cpu, number: "02", title: "Dhis BIOS", detail: "GCC/Clang wuxuu dhisaa kernel-ka freestanding 32-bit iyo boot image-ka." },
  { icon: Play, number: "03", title: "Tijaabi QEMU", detail: "Smoke test-ku wuxuu boot-gareeyaa image-ka, akhriyaa serial log, wuxuuna hubiyaa assertions-ka." },
  { icon: ShieldCheck, number: "04", title: "Hubi natiijada", detail: "FAT12, scheduler, memory isolation, identity iyo FScan security audit ayaa la xaqiijiyaa." },
];

export default function BuildGuidePage() {
  return (
    <div className="space-y-10">
      <section className="relative overflow-hidden border border-[#d2cabc] bg-[#f8f6f0] p-6 shadow-[8px_8px_0_#d6cfbf] md:p-10">
        <div className="absolute right-[-50px] top-[-70px] h-56 w-56 rounded-full border-[28px] border-[#f2c94c]/35" />
        <div className="relative max-w-3xl">
          <div className="mb-4 flex items-center gap-3 font-mono text-[10px] uppercase tracking-[0.25em] text-[#238a85]"><FileCode2 size={15} /> Build desk / shell reference</div>
          <h1 className="font-[var(--app-font-serif)] text-4xl font-bold tracking-[-0.05em] text-[#163243] md:text-6xl">Dhis, boot-garee,<br /><span className="text-[#238a85]">baar amniga.</span></h1>
          <p className="mt-5 max-w-2xl text-sm leading-7 text-[#647572]">Boggan wuxuu sharxayaa sida FadalWatch source-ka loo soo dejiyo, BIOS kernel loo dhiso, QEMU loogu tijaabiyo, iyo sida loo isticmaalo shell-ka gudaha Fadal kernel.</p>
          <div className="mt-7 flex flex-wrap gap-3"><span className="border border-[#b8dcd1] bg-[#e8f4ee] px-3 py-2 font-mono text-[10px] uppercase tracking-[0.14em] text-[#238a85]">BIOS / x86</span><span className="border border-[#e8d49b] bg-[#fff5d7] px-3 py-2 font-mono text-[10px] uppercase tracking-[0.14em] text-[#8c6c16]">QEMU smoke verified</span><span className="border border-[#d6d9d2] bg-[#f0f1ed] px-3 py-2 font-mono text-[10px] uppercase tracking-[0.14em] text-[#63716e]">ring-3 shell</span></div>
        </div>
      </section>

      <section>
        <div className="mb-5 flex items-end justify-between"><div><div className="font-mono text-[10px] uppercase tracking-[0.25em] text-[#238a85]">Execution path</div><h2 className="mt-2 font-[var(--app-font-serif)] text-3xl font-bold tracking-[-0.04em]">Habka dhismaha</h2></div><span className="hidden font-mono text-[10px] uppercase tracking-[0.16em] text-[#84918d] md:block">01 — 04 / verified flow</span></div>
        <div className="grid gap-4 md:grid-cols-4">{pipeline.map(({ icon: Icon, number, title, detail }) => <div key={number} className="border border-[#d9d3c7] bg-[#f8f6f0] p-5"><div className="flex items-start justify-between"><span className="font-mono text-2xl text-[#d0c8b8]">{number}</span><div className="flex h-9 w-9 items-center justify-center bg-[#ddf0ea] text-[#238a85]"><Icon size={17} /></div></div><h3 className="mt-7 font-semibold text-[#244455]">{title}</h3><p className="mt-2 text-xs leading-5 text-[#75837f]">{detail}</p></div>)}</div>
      </section>

      <section className="grid gap-6 lg:grid-cols-[1.1fr_0.9fr]">
        <div className="space-y-6"><div><div className="mb-3 font-mono text-[10px] uppercase tracking-[0.25em] text-[#238a85]">One-command path</div><h2 className="font-[var(--app-font-serif)] text-3xl font-bold tracking-[-0.04em]">Soo dejin + build + test</h2><p className="mt-3 text-sm leading-6 text-[#71807c]">Termux, Debian ama Linux kale ku orod amarkan. `--no-uefi` wuxuu ka boodaa UEFI haddii toolchain-ka GNU-EFI uusan jirin.</p></div><CodeBlock label="terminal / all-in-one" value={downloadCommand} /></div>
        <div className="border border-[#d9d3c7] bg-[#163243] p-6 text-[#e7eee5] shadow-[6px_6px_0_#d6cfbf]"><div className="flex items-center gap-2 font-mono text-[10px] uppercase tracking-[0.2em] text-[#f2c94c]"><ShieldCheck size={15} /> What gets verified</div><ul className="mt-6 space-y-4 text-sm leading-6 text-[#c7d7d2]"><li><strong className="text-white">Independence guard</strong><br />No host headers or unresolved runtime symbols.</li><li><strong className="text-white">Boot smoke</strong><br />Fadal Kernel reaches the shell through QEMU serial output.</li><li><strong className="text-white">Security trace</strong><br /><code className="text-[#f2c94c]">FScan audit passed</code> and loopback boundary verified.</li><li><strong className="text-white">Identity ABI</strong><br /><code className="text-[#f2c94c]">getuid</code>, <code className="text-[#f2c94c]">getgid</code>, and <code className="text-[#f2c94c]">getcaps</code> dispatch confirmed.</li></ul></div>
      </section>

      <section className="grid gap-6 lg:grid-cols-2"><CodeBlock label="manual / build" value={buildCommand} /><CodeBlock label="manual / smoke test" value={testCommand} /></section>

      <section className="grid gap-6 lg:grid-cols-[0.85fr_1.15fr]">
        <div className="border border-[#d9d3c7] bg-[#f8f6f0] p-6"><div className="flex items-center gap-2 font-mono text-[10px] uppercase tracking-[0.2em] text-[#238a85]"><Terminal size={15} /> Shell boundary</div><h2 className="mt-3 font-[var(--app-font-serif)] text-3xl font-bold tracking-[-0.04em]">Laba prompt ha isku khaldin</h2><p className="mt-4 text-sm leading-6 text-[#71807c]"><code className="font-mono text-[#c44f3c]">termiux:~#</code> waa Debian/Termux. <code className="font-mono text-[#238a85]">fadal&gt;</code> wuxuu muuqdaa oo keliya gudaha QEMU kadib boot-ka FadalWatch.</p><div className="mt-5 border-l-2 border-[#f2c94c] bg-[#fff7dc] p-4 text-xs leading-5 text-[#74602a]">QEMU bilow marka hore, sug <code>fadal&gt;</code>, kadib qor command-ka adigoon qorin prompt-ka laftiisa.</div></div>
        <CodeBlock label="fadal shell / commands" value={shellCommands} />
      </section>

      <section className="border border-[#d9d3c7] bg-[#f8f6f0] p-6 md:p-8"><div className="flex flex-col justify-between gap-5 md:flex-row md:items-center"><div><div className="font-mono text-[10px] uppercase tracking-[0.25em] text-[#238a85]">Expected security result</div><h2 className="mt-2 font-[var(--app-font-serif)] text-3xl font-bold tracking-[-0.04em]">FScan audit passed</h2><p className="mt-2 max-w-2xl text-sm leading-6 text-[#71807c]">Marka aad ku qorto <code className="font-mono text-[#238a85]">fscan</code> gudaha <code className="font-mono text-[#238a85]">fadal&gt;</code>, natiijada la filayo waa boundary loopback-only ah oo aan dirin packets dibadda.</p></div><div className="flex h-14 w-14 shrink-0 items-center justify-center border border-[#a9d7c9] bg-[#ddf0ea] text-[#238a85]"><Check size={27} strokeWidth={2.5} /></div></div><div className="mt-6 border border-[#b8dcd1] bg-[#e8f4ee] px-4 py-4 font-mono text-xs text-[#176f6a]">security: FScan audit passed; loopback-only boundary verified</div></section>
    </div>
  );
}
