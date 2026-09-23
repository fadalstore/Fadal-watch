import { useMemo, useState, type ReactNode } from "react";
import { Link, useLocation, useParams } from "wouter";
import { useQueryClient } from "@tanstack/react-query";
import {
  Activity as ActivityIcon,
  AlertTriangle,
  BadgeCheck,
  Bell,
  Check,
  ChevronRight,
  Clock3,
  Crosshair,
  Database,
  Eye,
  FileSearch,
  Filter,
  Gauge,
  Globe2,
  Layers3,
  Menu,
  Network,
  Plus,
  Search,
  Server,
  Shield,
  ShieldCheck,
  Target,
  X,
  Zap,
} from "lucide-react";
import {
  AlertUpdateState,
  type Activity,
  type Alert,
  type Asset,
  type AssetDetail,
  type AssetInputType,
  type CoverageZone,
  getGetAssetQueryKey,
  getGetCoverageQueryKey,
  getGetOverviewQueryKey,
  getListActivityQueryKey,
  getListAlertsQueryKey,
  getListAssetsQueryKey,
  useCreateAsset,
  useCreateScanRun,
  useGetAsset,
  useGetCoverage,
  useGetOverview,
  useListActivity,
  useListAlerts,
  useListAssets,
  useUpdateAlert,
} from "@workspace/api-client-react";

const navItems = [
  { href: "/", label: "Posture", icon: Gauge },
  { href: "/assets", label: "Assets", icon: Server },
  { href: "/alerts", label: "Alerts", icon: Bell },
  { href: "/activity", label: "Activity", icon: ActivityIcon },
  { href: "/coverage", label: "Coverage", icon: Network },
  { href: "/build-guide", label: "Build guide", icon: FileSearch },
];

const cx = (...classes: Array<string | false | null | undefined>) => classes.filter(Boolean).join(" ");

const formatTime = (value?: string | null) => {
  if (!value) return "Never";
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return value;
  return new Intl.DateTimeFormat("en", { month: "short", day: "numeric", hour: "numeric", minute: "2-digit" }).format(date);
};

const relativeTime = (value?: string | null) => {
  if (!value) return "No observation";
  const delta = Date.now() - new Date(value).getTime();
  const minutes = Math.max(0, Math.round(delta / 60000));
  if (minutes < 2) return "Just now";
  if (minutes < 60) return `${minutes}m ago`;
  if (minutes < 1440) return `${Math.round(minutes / 60)}h ago`;
  return `${Math.round(minutes / 1440)}d ago`;
};

const statusTone = (status: string) => ({
  healthy: "text-teal-700 bg-teal-50 border-teal-200",
  watch: "text-amber-800 bg-amber-50 border-amber-200",
  critical: "text-red-700 bg-red-50 border-red-200",
  open: "text-red-700 bg-red-50 border-red-200",
  acknowledged: "text-amber-800 bg-amber-50 border-amber-200",
  resolved: "text-teal-700 bg-teal-50 border-teal-200",
  stable: "text-teal-700 bg-teal-50 border-teal-200",
  learning: "text-slate-700 bg-slate-100 border-slate-200",
  drift: "text-red-700 bg-red-50 border-red-200",
  expected: "text-teal-700 bg-teal-50 border-teal-200",
  new: "text-amber-800 bg-amber-50 border-amber-200",
  risky: "text-red-700 bg-red-50 border-red-200",
}[status] || "text-slate-700 bg-slate-100 border-slate-200");

const severityTone = (severity: string) => ({
  critical: "bg-red-500",
  high: "bg-orange-500",
  medium: "bg-amber-400",
  low: "bg-teal-500",
  info: "bg-slate-400",
}[severity] || "bg-slate-400");

function Mark() {
  return (
    <div className="relative flex h-9 w-9 items-center justify-center border border-[#e2b849]/60 bg-[#f2c94c] text-[#102b3b] shadow-[3px_3px_0_#0e2b3d]">
      <Crosshair size={20} strokeWidth={2.5} />
      <span className="absolute -right-1 -top-1 h-2 w-2 rounded-full bg-[#ef6c52]" />
    </div>
  );
}

export function AppShell({ children }: { children: ReactNode }) {
  const [location] = useLocation();
  const [mobileOpen, setMobileOpen] = useState(false);
  const current = navItems.find((item) => item.href === location)?.label ?? (location.startsWith("/assets/") ? "Asset detail" : "Guardian");
  return (
    <div className="min-h-[100dvh] bg-[#eee9df] text-[#163243]">
      <aside className={cx("fixed inset-y-0 left-0 z-40 flex w-[248px] flex-col bg-[#102b3b] px-5 py-5 text-[#e9e4d8] transition-transform duration-300 md:translate-x-0", mobileOpen ? "translate-x-0" : "-translate-x-full")}>
        <div className="flex items-center gap-3 px-1">
          <Mark />
          <div>
            <div className="font-[var(--app-font-serif)] text-[15px] font-bold tracking-[0.18em]">FADALWATCH</div>
            <div className="font-mono text-[9px] uppercase tracking-[0.28em] text-[#87a6ad]">Network Guardian</div>
          </div>
        </div>
        <div className="mt-10 border-y border-[#36505a] py-4">
          <div className="font-mono text-[9px] uppercase tracking-[0.22em] text-[#74939c]">Workspace</div>
          <div className="mt-2 flex items-center gap-2 text-xs text-[#d8e2dc]"><span className="h-2 w-2 rounded-full bg-[#63c8b0]" /> authorized / field-ops</div>
        </div>
        <nav className="mt-7 space-y-1">
          {navItems.map((item) => {
            const active = item.href === "/" ? location === "/" : location.startsWith(item.href);
            const Icon = item.icon;
            return (
              <Link key={item.href} href={item.href} onClick={() => setMobileOpen(false)} data-testid={`link-nav-${item.label.toLowerCase()}`} className={cx("group flex items-center gap-3 border-l-2 px-3 py-3 text-sm font-semibold transition-colors", active ? "border-[#f2c94c] bg-[#1e4050] text-[#f2c94c]" : "border-transparent text-[#98b0b3] hover:border-[#527a7c] hover:bg-[#173747] hover:text-[#edf1e9]")}>
                <Icon size={17} strokeWidth={active ? 2.4 : 1.8} />
                <span>{item.label}</span>
                {item.label === "Alerts" && <span className="ml-auto rounded-sm bg-[#ef6c52] px-1.5 py-0.5 font-mono text-[9px] text-white">LIVE</span>}
              </Link>
            );
          })}
        </nav>
        <div className="mt-auto border-t border-[#36505a] pt-4">
          <div className="flex items-center justify-between font-mono text-[9px] uppercase tracking-[0.14em] text-[#74939c]"><span>Agent status</span><span className="text-[#63c8b0]">online</span></div>
          <div className="mt-3 flex items-center gap-2 text-xs text-[#d8e2dc]"><span className="h-2 w-2 rounded-full bg-[#63c8b0]" /> Evidence relay connected</div>
          <div className="mt-5 text-[10px] leading-4 text-[#6d8b94]">Observe only. Every action is scoped to assets you own or are authorized to assess.</div>
        </div>
      </aside>
      {mobileOpen && <button aria-label="Close navigation" onClick={() => setMobileOpen(false)} className="fixed inset-0 z-30 bg-[#102b3b]/45 md:hidden" data-testid="button-close-navigation" />}
      <main className="min-h-[100dvh] md:pl-[248px]">
        <header className="sticky top-0 z-20 flex h-[72px] items-center justify-between border-b border-[#ddd7cb] bg-[#eee9df]/95 px-5 backdrop-blur md:px-10">
          <div className="flex items-center gap-3">
            <button onClick={() => setMobileOpen(true)} className="md:hidden" data-testid="button-open-navigation"><Menu size={21} /></button>
            <div className="font-mono text-[10px] uppercase tracking-[0.25em] text-[#6c817f]">Guardian / <span className="text-[#163243]">{current}</span></div>
          </div>
          <div className="flex items-center gap-4">
            <span className="hidden items-center gap-2 font-mono text-[10px] uppercase tracking-[0.14em] text-[#6c817f] sm:flex"><span className="h-2 w-2 rounded-full bg-[#63c8b0]" /> telemetry current</span>
            <div className="flex h-8 w-8 items-center justify-center rounded-full bg-[#163243] font-mono text-[10px] font-bold text-[#f2c94c]">FW</div>
          </div>
        </header>
        <div className="mx-auto max-w-[1460px] px-5 py-8 md:px-10 md:py-10">{children}</div>
      </main>
    </div>
  );
}

function PageIntro({ eyebrow, title, detail, action }: { eyebrow: string; title: string; detail: string; action?: ReactNode }) {
  return <div className="mb-8 flex flex-col justify-between gap-5 lg:flex-row lg:items-end">
    <div><div className="mb-3 font-mono text-[10px] uppercase tracking-[0.25em] text-[#238a85]">{eyebrow}</div><h1 className="font-[var(--app-font-serif)] text-4xl font-bold tracking-[-0.04em] text-[#163243] md:text-5xl">{title}</h1><p className="mt-3 max-w-[680px] text-sm leading-6 text-[#6b7d7c]">{detail}</p></div>
    {action}
  </div>;
}

function Panel({ children, className = "" }: { children: ReactNode; className?: string }) {
  return <section className={cx("border border-[#d9d3c7] bg-[#f8f6f0] shadow-[0_2px_0_rgba(22,50,67,0.05)]", className)}>{children}</section>;
}

function SectionLabel({ children, count }: { children: ReactNode; count?: string | number }) {
  return <div className="flex items-center justify-between border-b border-[#ded9ce] px-5 py-4"><h2 className="font-mono text-[10px] font-medium uppercase tracking-[0.2em] text-[#657978]">{children}</h2>{count !== undefined && <span className="font-mono text-[10px] text-[#9aa5a0]">{count}</span>}</div>;
}

function StatCard({ label, value, detail, accent = "teal", icon: Icon }: { label: string; value: string | number; detail: string; accent?: "teal" | "yellow" | "red"; icon: typeof Gauge }) {
  const colors = { teal: "text-[#238a85] bg-[#ddf0ea]", yellow: "text-[#9e7611] bg-[#f8e9b7]", red: "text-[#c44f3c] bg-[#f8dbd3]" };
  return <div className="border border-[#d9d3c7] bg-[#f8f6f0] p-5 shadow-[0_2px_0_rgba(22,50,67,0.04)]"><div className="flex items-start justify-between"><div className="font-mono text-[10px] uppercase tracking-[0.18em] text-[#728380]">{label}</div><div className={cx("flex h-8 w-8 items-center justify-center", colors[accent])}><Icon size={16} /></div></div><div className="mt-5 font-[var(--app-font-serif)] text-3xl font-bold tracking-[-0.04em]">{value}</div><div className="mt-2 text-xs text-[#74827f]">{detail}</div></div>;
}

export function LoadingState({ label = "Loading evidence" }: { label?: string }) {
  return <div className="space-y-4" data-testid="state-loading"><div className="h-24 animate-pulse bg-[#e3ded3]" /><div className="h-48 animate-pulse bg-[#e3ded3]" /><div className="font-mono text-[10px] uppercase tracking-[0.18em] text-[#82908b]">{label}...</div></div>;
}

export function ErrorState({ retry }: { retry?: () => void }) {
  return <div className="border border-[#e9b7ac] bg-[#fff2ef] p-6" data-testid="state-error"><div className="flex items-center gap-3 text-[#b34737]"><AlertTriangle size={18} /><span className="font-semibold">Evidence relay unavailable</span></div><p className="mt-2 text-sm text-[#8c665f]">The workspace could not read the latest network state.</p>{retry && <button onClick={retry} className="mt-4 border border-[#b34737] px-3 py-2 font-mono text-[10px] uppercase tracking-[0.14em] text-[#b34737]" data-testid="button-retry">Retry relay</button>}</div>;
}

function EmptyState({ title, detail, action }: { title: string; detail: string; action?: ReactNode }) {
  return <div className="flex min-h-[230px] flex-col items-center justify-center border border-dashed border-[#c8c2b7] bg-[#f5f2ea] px-6 text-center" data-testid="state-empty"><div className="flex h-12 w-12 items-center justify-center border border-[#c8c2b7] text-[#7e918b]"><Database size={20} /></div><h3 className="mt-4 font-[var(--app-font-serif)] text-xl font-bold">{title}</h3><p className="mt-2 max-w-sm text-sm text-[#77847f]">{detail}</p>{action && <div className="mt-5">{action}</div>}</div>;
}

function ScoreRing({ score }: { score: number }) {
  const radius = 65;
  const circumference = 2 * Math.PI * radius;
  return <div className="relative h-44 w-44 shrink-0"><svg viewBox="0 0 160 160" className="-rotate-90"><circle cx="80" cy="80" r={radius} fill="none" stroke="#d9e0d8" strokeWidth="10" /><circle cx="80" cy="80" r={radius} fill="none" stroke="#238a85" strokeWidth="10" strokeLinecap="square" strokeDasharray={circumference} strokeDashoffset={circumference * (1 - score / 100)} /></svg><div className="absolute inset-0 flex flex-col items-center justify-center"><span className="font-[var(--app-font-serif)] text-5xl font-bold tracking-[-0.08em]">{score}</span><span className="font-mono text-[9px] uppercase tracking-[0.16em] text-[#71827e]">health score</span></div></div>;
}

function ActivityRow({ item }: { item: Activity }) {
  return <div className="group flex gap-4 border-b border-[#e2ddd3] py-4 last:border-0" data-testid={`activity-row-${item.id}`}><div className="relative flex w-5 shrink-0 justify-center"><span className={cx("mt-1.5 h-2.5 w-2.5 rounded-full ring-4 ring-[#f8f6f0]", severityTone(item.severity))} /></div><div className="min-w-0 flex-1"><div className="flex flex-wrap items-center gap-2"><span className="text-sm font-semibold text-[#244455]">{item.title}</span><span className={cx("border px-1.5 py-0.5 font-mono text-[9px] uppercase tracking-[0.08em]", statusTone(item.severity))}>{item.severity}</span></div><p className="mt-1 text-xs leading-5 text-[#75837f]">{item.detail}</p><div className="mt-2 flex flex-wrap gap-3 font-mono text-[9px] uppercase tracking-[0.08em] text-[#9aa49e]"><span>{item.kind}</span><span>{formatTime(item.timestamp)}</span>{item.assetId && <Link href={`/assets/${item.assetId}`} className="text-[#238a85] hover:underline" data-testid={`link-activity-asset-${item.id}`}>inspect asset <ChevronRight size={11} className="inline" /></Link>}</div></div></div>;
}

function TrendChart({ trend }: { trend: Array<{ label: string; score: number }> }) {
  const max = Math.max(...trend.map((item) => item.score), 100);
  return <div className="flex h-48 items-end gap-2 border-b border-l border-[#d5dcd3] px-3 pb-0 pt-5">{trend.map((item) => <div key={item.label} className="group flex h-full flex-1 flex-col items-center justify-end gap-2"><div className="relative w-full max-w-[38px] bg-[#63c8b0] transition-transform duration-300 group-hover:-translate-y-1" style={{ height: `${Math.max((item.score / max) * 100, 6)}%` }}><span className="absolute -top-5 left-1/2 -translate-x-1/2 font-mono text-[9px] text-[#55706d]">{item.score}</span></div><span className="font-mono text-[9px] uppercase text-[#8a9690]">{item.label}</span></div>)}</div>;
}

function ScanDialog({ open, onClose }: { open: boolean; onClose: () => void }) {
  const queryClient = useQueryClient();
  const scan = useCreateScanRun();
  const [scope, setScope] = useState("");
  const [confirmed, setConfirmed] = useState(false);
  if (!open) return null;
  const submit = () => {
    if (!scope.trim() || !confirmed) return;
    scan.mutate({ data: { scope: scope.trim(), authorizationConfirmed: true, mode: "observation" } }, { onSuccess: () => { queryClient.invalidateQueries({ queryKey: getGetOverviewQueryKey() }); queryClient.invalidateQueries({ queryKey: getListAssetsQueryKey() }); queryClient.invalidateQueries({ queryKey: getListActivityQueryKey() }); queryClient.invalidateQueries({ queryKey: getGetCoverageQueryKey() }); queryClient.invalidateQueries({ queryKey: getListAlertsQueryKey() }); onClose(); setScope(""); setConfirmed(false); } });
  };
  return <div className="fixed inset-0 z-50 flex items-center justify-center bg-[#102b3b]/55 p-5"><div className="w-full max-w-lg border border-[#d9d3c7] bg-[#f8f6f0] p-6 shadow-[10px_10px_0_#102b3b]" role="dialog" aria-modal="true" data-testid="dialog-observation"><div className="flex items-start justify-between"><div><div className="font-mono text-[10px] uppercase tracking-[0.2em] text-[#238a85]">Safe observation</div><h2 className="mt-2 font-[var(--app-font-serif)] text-2xl font-bold">Start a scoped run</h2></div><button onClick={onClose} data-testid="button-close-scan"><X size={18} /></button></div><p className="mt-4 text-sm leading-6 text-[#687a77]">Observe only. No credentials, exploitation, or modification. Confirm the scope before Guardian collects evidence.</p><label className="mt-6 block font-mono text-[10px] uppercase tracking-[0.16em] text-[#637875]">Authorized scope<input value={scope} onChange={(event) => setScope(event.target.value)} placeholder="e.g. 10.24.0.0/24 or one IPv4 address" className="mt-2 w-full border border-[#cfc8bb] bg-[#fffdf8] px-3 py-3 font-mono text-sm outline-none focus:border-[#238a85]" data-testid="input-scan-scope" /></label><p className="mt-2 text-xs text-[#77847f]">CIDR runs are limited to 64 addresses so each observation stays deliberate.</p><label className="mt-5 flex cursor-pointer items-start gap-3 text-sm text-[#405c5c]"><input type="checkbox" checked={confirmed} onChange={(event) => setConfirmed(event.target.checked)} className="mt-0.5 accent-[#238a85]" data-testid="input-scan-authorization" /><span>I confirm this scope is owned by me or explicitly authorized for assessment.</span></label>{scan.isError && <p className="mt-4 border border-[#e9b7ac] bg-[#fff2ef] px-3 py-2 text-xs text-[#b34737]">Observation could not start. Use one IPv4 address or a CIDR with at most 64 addresses.</p>}<div className="mt-7 flex justify-end gap-3"><button onClick={onClose} className="px-4 py-2 font-mono text-[10px] uppercase tracking-[0.14em] text-[#687a77]" data-testid="button-cancel-scan">Cancel</button><button disabled={!scope.trim() || !confirmed || scan.isPending} onClick={submit} className="bg-[#f2c94c] px-4 py-2 font-mono text-[10px] font-bold uppercase tracking-[0.14em] text-[#102b3b] disabled:cursor-not-allowed disabled:opacity-50" data-testid="button-start-scan">{scan.isPending ? "Observing..." : "Start observation"}</button></div></div></div>;
}

export function OverviewPage() {
  const overviewQuery = useGetOverview();
  const [scanOpen, setScanOpen] = useState(false);
  const overview = overviewQuery.data;
  if (overviewQuery.isLoading) return <LoadingState label="Loading posture" />;
  if (overviewQuery.isError || !overview) return <ErrorState retry={() => overviewQuery.refetch()} />;
  return <><PageIntro eyebrow="01 / network posture" title="Know what changed." detail="A measured view of your authorized network. Guardian turns drift, exposure, and new observations into evidence you can act on." action={<button onClick={() => setScanOpen(true)} className="flex items-center justify-center gap-2 bg-[#f2c94c] px-5 py-3 font-mono text-[10px] font-bold uppercase tracking-[0.14em] text-[#102b3b] shadow-[4px_4px_0_#102b3b] transition-transform hover:translate-x-0.5 hover:translate-y-0.5" data-testid="button-quick-observation"><Eye size={15} /> Quick observation</button>} /><div className="grid gap-4 sm:grid-cols-2 xl:grid-cols-4"><StatCard label="Monitored assets" value={overview.monitoredAssets} detail={`${overview.newToday} new in the last day`} accent="teal" icon={Server} /><StatCard label="Active alert pressure" value={overview.activeAlerts} detail="Requires operator review" accent={overview.activeAlerts ? "red" : "teal"} icon={Bell} /><StatCard label="Exposed services" value={overview.exposedServices} detail="Observed outside baseline" accent={overview.exposedServices ? "yellow" : "teal"} icon={Globe2} /><StatCard label="Coverage" value={`${overview.coverage}%`} detail="Across monitored zones" accent="teal" icon={Target} /></div><div className="mt-4 grid gap-4 xl:grid-cols-[1.35fr_1fr]"><Panel className="p-6"><div className="flex flex-col items-center gap-7 sm:flex-row"><ScoreRing score={overview.healthScore} /><div className="flex-1"><div className="font-mono text-[10px] uppercase tracking-[0.2em] text-[#238a85]">Posture signal</div><h2 className="mt-2 font-[var(--app-font-serif)] text-2xl font-bold">Stable with edges to watch.</h2><p className="mt-3 max-w-md text-sm leading-6 text-[#71817d]">Health is a weighted read of baseline stability, exposure, and monitoring coverage. It is a signal, not a verdict.</p><div className="mt-5 flex items-center gap-3 border-t border-[#ded9ce] pt-4 font-mono text-[10px] uppercase tracking-[0.12em] text-[#7f8d88]"><Clock3 size={14} /> Last scan <span className="text-[#163243]">{formatTime(overview.lastScan)}</span></div></div></div></Panel><Panel><SectionLabel>Asset composition</SectionLabel><div className="space-y-4 p-6">{overview.assetsByType.map((item) => <div key={item.type} className="flex items-center gap-3" data-testid={`metric-asset-type-${item.type}`}><div className="w-24 font-mono text-[10px] uppercase tracking-[0.1em] text-[#72817d]">{item.type}</div><div className="h-2 flex-1 bg-[#dce5de]"><div className="h-full bg-[#238a85]" style={{ width: `${overview.monitoredAssets ? (item.count / overview.monitoredAssets) * 100 : 0}%` }} /></div><span className="w-7 text-right font-mono text-xs font-medium">{item.count}</span></div>)}</div></Panel></div><div className="mt-4 grid gap-4 xl:grid-cols-[1fr_1.2fr]"><Panel><SectionLabel>Health trend <span className="ml-2 text-[#b1b8b0]">7 observations</span></SectionLabel><div className="p-6"><TrendChart trend={overview.trend} /></div></Panel><Panel><SectionLabel count={overview.recentActivity.length}>Recent evidence</SectionLabel><div className="px-5">{overview.recentActivity.length ? overview.recentActivity.slice(0, 5).map((item) => <ActivityRow key={item.id} item={item} />) : <EmptyState title="Quiet network" detail="No recent evidence has been recorded yet." />}</div><div className="border-t border-[#ded9ce] px-5 py-4"><Link href="/activity" className="font-mono text-[10px] uppercase tracking-[0.15em] text-[#238a85] hover:underline" data-testid="link-view-all-activity">View full evidence timeline <ChevronRight size={13} className="inline" /></Link></div></Panel></div><ScanDialog open={scanOpen} onClose={() => setScanOpen(false)} /></>;
}

function AssetRow({ asset }: { asset: Asset }) {
  return <Link href={`/assets/${asset.id}`} className="grid grid-cols-[1.5fr_1fr_0.7fr_0.8fr_0.8fr] items-center gap-4 border-b border-[#e2ddd3] px-5 py-4 transition-colors hover:bg-[#f0ede5]" data-testid={`row-asset-${asset.id}`}><div className="min-w-0"><div className="flex items-center gap-2 font-semibold text-[#244455]"><span className="truncate">{asset.label}</span><ChevronRight size={14} className="shrink-0 text-[#a1aca6]" /></div><div className="mt-1 font-mono text-[10px] text-[#81908a]">{asset.address}</div></div><div><span className={cx("border px-2 py-1 font-mono text-[9px] uppercase tracking-[0.1em]", statusTone(asset.status))}>{asset.status}</span></div><div className="font-mono text-xs text-[#536a6b]">{asset.type}</div><div className="font-mono text-xs text-[#536a6b]">{asset.services} svc</div><div className="text-right"><div className="font-mono text-xs font-medium">{asset.risk}</div><div className="mt-1 text-[9px] uppercase text-[#8c9992]">risk</div></div></Link>;
}

export function AssetsPage() {
  const [search, setSearch] = useState("");
  const [status, setStatus] = useState<"all" | "healthy" | "watch" | "critical">("all");
  const [addOpen, setAddOpen] = useState(false);
  const query = useListAssets({ search: search || undefined, status });
  const assets = query.data || [];
  return <><PageIntro eyebrow="02 / inventory" title="Monitored assets" detail="The living inventory of authorized infrastructure. Search by address or label, then follow the evidence trail to a single asset." action={<button onClick={() => setAddOpen(true)} className="flex items-center justify-center gap-2 border border-[#163243] bg-[#163243] px-5 py-3 font-mono text-[10px] font-bold uppercase tracking-[0.14em] text-[#f2c94c]" data-testid="button-add-asset"><Plus size={15} /> Add authorized asset</button>} /><div className="mb-4 flex flex-col gap-3 border border-[#d9d3c7] bg-[#f8f6f0] p-3 md:flex-row"><label className="relative flex-1"><Search size={16} className="absolute left-3 top-3 text-[#8a9991]" /><input value={search} onChange={(event) => setSearch(event.target.value)} placeholder="Search label or address" className="w-full border border-[#d6d0c4] bg-[#fffdf8] py-2.5 pl-9 pr-3 text-sm outline-none focus:border-[#238a85]" data-testid="input-search-assets" /></label><div className="flex items-center gap-2"><Filter size={15} className="text-[#778782]" />{(["all", "healthy", "watch", "critical"] as const).map((item) => <button key={item} onClick={() => setStatus(item)} className={cx("border px-3 py-2 font-mono text-[9px] uppercase tracking-[0.1em]", status === item ? "border-[#238a85] bg-[#ddf0ea] text-[#16776f]" : "border-[#d6d0c4] text-[#75847f] hover:bg-[#eeebe3]")} data-testid={`button-filter-${item}`}>{item}</button>)}</div></div>{query.isLoading ? <LoadingState label="Loading asset inventory" /> : query.isError ? <ErrorState retry={() => query.refetch()} /> : <Panel><div className="hidden grid-cols-[1.5fr_1fr_0.7fr_0.8fr_0.8fr] gap-4 border-b border-[#ded9ce] px-5 py-3 font-mono text-[9px] uppercase tracking-[0.16em] text-[#89948e] md:grid"><span>Asset</span><span>Status</span><span>Type</span><span>Observed</span><span className="text-right">Risk</span></div><div className="hidden md:block">{assets.map((asset) => <AssetRow key={asset.id} asset={asset} />)}</div><div className="space-y-3 p-3 md:hidden">{assets.map((asset) => <Link href={`/assets/${asset.id}`} key={asset.id} className="block border border-[#e0dbd1] p-4" data-testid={`card-asset-${asset.id}`}><div className="flex justify-between gap-4"><div><div className="font-semibold">{asset.label}</div><div className="mt-1 font-mono text-[10px] text-[#81908a]">{asset.address}</div></div><span className={cx("h-fit border px-2 py-1 font-mono text-[9px] uppercase", statusTone(asset.status))}>{asset.status}</span></div><div className="mt-4 flex justify-between font-mono text-[10px] text-[#74827f]"><span>{asset.type}</span><span>{asset.services} services</span><span>risk {asset.risk}</span></div></Link>)}</div>{!assets.length && <EmptyState title="No assets match" detail="Adjust the search or add an asset you are authorized to monitor." action={<button onClick={() => setAddOpen(true)} className="border border-[#238a85] px-3 py-2 font-mono text-[10px] uppercase text-[#238a85]" data-testid="button-empty-add-asset">Add asset</button>} />}</Panel>}<AddAssetDialog open={addOpen} onClose={() => setAddOpen(false)} /></>;
}

function AddAssetDialog({ open, onClose }: { open: boolean; onClose: () => void }) {
  const queryClient = useQueryClient();
  const create = useCreateAsset();
  const [label, setLabel] = useState("");
  const [address, setAddress] = useState("");
  const [type, setType] = useState<AssetInputType>("server");
  const [zone, setZone] = useState("");
  if (!open) return null;
  const submit = () => create.mutate({ data: { label, address, type, zone: zone || undefined } }, { onSuccess: () => { queryClient.invalidateQueries({ queryKey: getListAssetsQueryKey() }); queryClient.invalidateQueries({ queryKey: getGetOverviewQueryKey() }); onClose(); setLabel(""); setAddress(""); setZone(""); } });
  return <div className="fixed inset-0 z-50 flex items-center justify-center bg-[#102b3b]/55 p-5"><div className="w-full max-w-lg border border-[#d9d3c7] bg-[#f8f6f0] p-6 shadow-[10px_10px_0_#102b3b]" role="dialog" aria-modal="true" data-testid="dialog-add-asset"><div className="flex justify-between"><div><div className="font-mono text-[10px] uppercase tracking-[0.2em] text-[#238a85]">Inventory intake</div><h2 className="mt-2 font-[var(--app-font-serif)] text-2xl font-bold">Add authorized asset</h2></div><button onClick={onClose} data-testid="button-close-add-asset"><X size={18} /></button></div><div className="mt-6 grid gap-4 sm:grid-cols-2"><label className="block text-xs font-semibold sm:col-span-2">Label<input value={label} onChange={(event) => setLabel(event.target.value)} placeholder="e.g. edge-router-01" className="mt-2 w-full border border-[#cfc8bb] bg-[#fffdf8] px-3 py-2.5 text-sm outline-none focus:border-[#238a85]" data-testid="input-asset-label" /></label><label className="block text-xs font-semibold">Address<input value={address} onChange={(event) => setAddress(event.target.value)} placeholder="10.24.1.12" className="mt-2 w-full border border-[#cfc8bb] bg-[#fffdf8] px-3 py-2.5 font-mono text-sm outline-none focus:border-[#238a85]" data-testid="input-asset-address" /></label><label className="block text-xs font-semibold">Type<select value={type} onChange={(event) => setType(event.target.value as AssetInputType)} className="mt-2 w-full border border-[#cfc8bb] bg-[#fffdf8] px-3 py-2.5 text-sm outline-none focus:border-[#238a85]" data-testid="select-asset-type"><option value="server">Server</option><option value="workstation">Workstation</option><option value="network">Network</option><option value="unknown">Unknown</option></select></label><label className="block text-xs font-semibold sm:col-span-2">Zone <span className="font-normal text-[#84918b]">optional</span><input value={zone} onChange={(event) => setZone(event.target.value)} placeholder="e.g. production / DMZ" className="mt-2 w-full border border-[#cfc8bb] bg-[#fffdf8] px-3 py-2.5 text-sm outline-none focus:border-[#238a85]" data-testid="input-asset-zone" /></label></div><div className="mt-7 flex justify-end gap-3"><button onClick={onClose} className="px-4 py-2 font-mono text-[10px] uppercase text-[#687a77]" data-testid="button-cancel-add-asset">Cancel</button><button disabled={!label.trim() || !address.trim() || create.isPending} onClick={submit} className="bg-[#f2c94c] px-4 py-2 font-mono text-[10px] font-bold uppercase text-[#102b3b] disabled:opacity-50" data-testid="button-submit-add-asset">{create.isPending ? "Adding..." : "Add asset"}</button></div></div></div>;
}

export function AssetDetailPage() {
  const { assetId = "" } = useParams<{ assetId: string }>();
  const query = useGetAsset(assetId, { query: { queryKey: getGetAssetQueryKey(assetId), enabled: Boolean(assetId) } });
  const asset = query.data as AssetDetail | undefined;
  if (query.isLoading) return <LoadingState label="Loading asset evidence" />;
  if (query.isError || !asset) return <ErrorState retry={() => query.refetch()} />;
  return <><PageIntro eyebrow="asset evidence" title={asset.label} detail={`${asset.address} · ${asset.zone} · first observed ${formatTime(asset.firstSeen)}`} action={<Link href="/assets" className="border border-[#c7c1b6] bg-[#f8f6f0] px-4 py-3 font-mono text-[10px] uppercase tracking-[0.14em] text-[#47615f]" data-testid="link-back-assets">Back to inventory</Link>} /><div className="grid gap-4 sm:grid-cols-2 xl:grid-cols-4"><StatCard label="Current status" value={asset.status} detail={`baseline ${asset.baselineState}`} accent={asset.status === "critical" ? "red" : asset.status === "watch" ? "yellow" : "teal"} icon={ShieldCheck} /><StatCard label="Exposure index" value={asset.exposure} detail="out of 100" accent={asset.exposure > 50 ? "yellow" : "teal"} icon={Globe2} /><StatCard label="Risk index" value={asset.risk} detail="weighted evidence" accent={asset.risk > 50 ? "red" : "teal"} icon={AlertTriangle} /><StatCard label="Services" value={asset.services} detail={`last seen ${relativeTime(asset.lastSeen)}`} accent="teal" icon={Zap} /></div><div className="mt-4 grid gap-4 xl:grid-cols-[1.1fr_1fr]"><Panel><SectionLabel count={asset.servicesList.length}>Observed services</SectionLabel><div>{asset.servicesList.length ? asset.servicesList.map((service) => <div key={`${service.name}-${service.port}`} className="flex flex-col gap-3 border-b border-[#e2ddd3] px-5 py-4 sm:flex-row sm:items-center"><div className="flex h-9 w-9 items-center justify-center bg-[#e3f0ea] text-[#238a85]"><Server size={16} /></div><div className="min-w-0 flex-1"><div className="font-semibold">{service.name} <span className="ml-1 font-mono text-xs font-normal text-[#82908b]">:{service.port}</span></div><div className="mt-1 text-xs text-[#75837f]">{service.note || "No operator note attached."}</div></div><div className="text-left sm:text-right"><span className={cx("border px-2 py-1 font-mono text-[9px] uppercase", statusTone(service.state))}>{service.state}</span><div className="mt-2 font-mono text-[9px] text-[#97a19b]">{relativeTime(service.lastObserved)}</div></div></div>) : <EmptyState title="No services observed" detail="An observation run has not recorded service evidence for this asset." />}</div></Panel><Panel><SectionLabel>Baseline posture</SectionLabel><div className="p-6"><div className="flex items-center gap-4"><div className={cx("flex h-14 w-14 items-center justify-center border", statusTone(asset.baselineState))}><Shield size={23} /></div><div><div className="font-[var(--app-font-serif)] text-2xl font-bold capitalize">{asset.baselineState}</div><p className="mt-1 text-sm text-[#75837f]">Current comparison state for this asset.</p></div></div><div className="mt-7 border-t border-[#ded9ce] pt-5"><div className="flex justify-between font-mono text-[10px] uppercase tracking-[0.12em] text-[#7b8984]"><span>Exposure</span><span>{asset.exposure}/100</span></div><div className="mt-2 h-2 bg-[#dee7df]"><div className={cx("h-full", asset.exposure > 65 ? "bg-[#ef6c52]" : "bg-[#f2c94c]")} style={{ width: `${asset.exposure}%` }} /></div><div className="mt-5 flex justify-between font-mono text-[10px] uppercase tracking-[0.12em] text-[#7b8984]"><span>Risk</span><span>{asset.risk}/100</span></div><div className="mt-2 h-2 bg-[#dee7df]"><div className={cx("h-full", asset.risk > 65 ? "bg-[#ef6c52]" : "bg-[#238a85]")} style={{ width: `${asset.risk}%` }} /></div></div></div></Panel></div><Panel className="mt-4"><SectionLabel count={asset.history.length}>Asset history</SectionLabel><div className="px-5">{asset.history.length ? asset.history.map((item) => <ActivityRow key={item.id} item={item} />) : <EmptyState title="No history yet" detail="Evidence will appear here after this asset is observed." />}</div></Panel></>;
}

export function AlertsPage() {
  const queryClient = useQueryClient();
  const [filter, setFilter] = useState<"all" | "open" | "acknowledged" | "resolved">("all");
  const query = useListAlerts({ state: filter });
  const update = useUpdateAlert();
  const alerts = query.data || [];
  const changeState = (alert: Alert, state: AlertUpdateState) => update.mutate({ alertId: alert.id, data: { state } }, { onSuccess: () => { queryClient.invalidateQueries({ queryKey: getListAlertsQueryKey() }); queryClient.invalidateQueries({ queryKey: getGetOverviewQueryKey() }); } });
  return <><PageIntro eyebrow="03 / explainable queue" title="Alerts with receipts." detail="Every alert is a claim with a reason, evidence, and a safe next step. Acknowledge what you understand; resolve what you have verified." /><div className="mb-4 flex flex-wrap gap-2">{(["all", "open", "acknowledged", "resolved"] as const).map((item) => <button key={item} onClick={() => setFilter(item)} className={cx("border px-3 py-2 font-mono text-[10px] uppercase tracking-[0.12em]", filter === item ? "border-[#238a85] bg-[#ddf0ea] text-[#16776f]" : "border-[#d6d0c4] bg-[#f8f6f0] text-[#75847f]")} data-testid={`button-alert-filter-${item}`}>{item}</button>)}</div>{query.isLoading ? <LoadingState label="Loading alert queue" /> : query.isError ? <ErrorState retry={() => query.refetch()} /> : alerts.length ? <div className="space-y-3">{alerts.map((alert) => <AlertCard key={alert.id} alert={alert} busy={update.isPending} onChange={changeState} />)}</div> : <EmptyState title="No alerts in this view" detail="Guardian has no alert evidence matching this state filter." />}</>;
}

function AlertCard({ alert, busy, onChange }: { alert: Alert; busy: boolean; onChange: (alert: Alert, state: AlertUpdateState) => void }) {
  return <Panel className="overflow-hidden" ><div className="flex flex-col gap-4 border-l-4 border-[#ef6c52] p-5 md:flex-row md:items-start"><div className="flex flex-1 gap-4"><div className={cx("mt-1 flex h-9 w-9 shrink-0 items-center justify-center text-white", severityTone(alert.severity))}><AlertTriangle size={17} /></div><div className="min-w-0 flex-1"><div className="flex flex-wrap items-center gap-2"><h2 className="font-[var(--app-font-serif)] text-xl font-bold">{alert.title}</h2><span className={cx("border px-2 py-1 font-mono text-[9px] uppercase", statusTone(alert.state))}>{alert.state}</span></div><p className="mt-2 text-sm leading-6 text-[#5e726f]">{alert.reason}</p><div className="mt-4 grid gap-3 md:grid-cols-2"><div className="border border-[#e1dcd2] bg-[#f4f1e9] p-3"><div className="font-mono text-[9px] uppercase tracking-[0.15em] text-[#82908b]">Recommendation</div><div className="mt-2 text-xs leading-5 text-[#4e6666]">{alert.recommendation}</div></div><div className="border border-[#e1dcd2] bg-[#f4f1e9] p-3"><div className="font-mono text-[9px] uppercase tracking-[0.15em] text-[#82908b]">Evidence</div>{alert.evidence?.length ? <ul className="mt-2 space-y-1 text-xs leading-5 text-[#4e6666]">{alert.evidence.map((line) => <li key={line} className="flex gap-2"><span className="text-[#238a85]">—</span>{line}</li>)}</ul> : <div className="mt-2 text-xs text-[#7d8a85]">Evidence details pending.</div>}</div></div><div className="mt-4 flex flex-wrap items-center gap-3 font-mono text-[9px] uppercase tracking-[0.1em] text-[#9aa49e]"><Link href={`/assets/${alert.assetId}`} className="text-[#238a85] hover:underline" data-testid={`link-alert-asset-${alert.id}`}>asset {alert.assetId}</Link><span>{formatTime(alert.createdAt)}</span><span>{alert.severity}</span></div></div></div><div className="flex shrink-0 gap-2 md:flex-col">{alert.state === "open" && <button disabled={busy} onClick={() => onChange(alert, AlertUpdateState.acknowledged)} className="flex items-center justify-center gap-2 border border-[#d5b24b] px-3 py-2 font-mono text-[9px] uppercase tracking-[0.1em] text-[#886b13] hover:bg-[#fff6d8] disabled:opacity-50" data-testid={`button-acknowledge-alert-${alert.id}`}><Eye size={14} /> Acknowledge</button>}{alert.state !== "resolved" && <button disabled={busy} onClick={() => onChange(alert, AlertUpdateState.resolved)} className="flex items-center justify-center gap-2 border border-[#238a85] px-3 py-2 font-mono text-[9px] uppercase tracking-[0.1em] text-[#16776f] hover:bg-[#e6f4ee] disabled:opacity-50" data-testid={`button-resolve-alert-${alert.id}`}><Check size={14} /> Resolve</button>}{alert.state === "resolved" && <span className="flex items-center gap-2 px-3 py-2 font-mono text-[9px] uppercase text-[#238a85]"><BadgeCheck size={14} /> Closed</span>}</div></div></Panel>;
}

export function ActivityPage() {
  const query = useListActivity({ limit: 100 });
  const items = useMemo(() => [...(query.data || [])].sort((a, b) => new Date(b.timestamp).getTime() - new Date(a.timestamp).getTime()), [query.data]);
  return <><PageIntro eyebrow="04 / evidence ledger" title="Activity timeline" detail="A chronological record of what Guardian observed, when it happened, and how much attention it deserves." /><div className="grid gap-4 xl:grid-cols-[0.72fr_1.28fr]"><Panel><SectionLabel>Evidence legend</SectionLabel><div className="space-y-4 p-5">{[["scan", "Observation runs and collection"], ["baseline", "Baseline comparisons"], ["alert", "New or changed signals"], ["response", "Operator state changes"], ["asset", "Inventory changes"]].map(([kind, detail]) => <div key={kind} className="flex gap-3"><div className="mt-1.5 h-2.5 w-2.5 rounded-full bg-[#238a85]" /><div><div className="font-mono text-[10px] uppercase tracking-[0.14em]">{kind}</div><div className="mt-1 text-xs text-[#7b8984]">{detail}</div></div></div>)}</div><div className="border-t border-[#ded9ce] p-5"><div className="flex items-center gap-2 font-mono text-[10px] uppercase tracking-[0.14em] text-[#73837e]"><FileSearch size={15} /> Scope: last 100 events</div></div></Panel><Panel><SectionLabel count={items.length}>Chronological record</SectionLabel><div className="px-5">{query.isLoading ? <LoadingState label="Loading timeline" /> : query.isError ? <ErrorState retry={() => query.refetch()} /> : items.length ? items.map((item) => <ActivityRow key={item.id} item={item} />) : <EmptyState title="No activity recorded" detail="Run an authorized observation to begin building evidence." />}</div></Panel></div></>;
}

export function CoveragePage() {
  const query = useGetCoverage();
  const zones = query.data || [];
  const totalMonitored = zones.reduce((sum, zone) => sum + zone.monitored, 0);
  const totalDiscovered = zones.reduce((sum, zone) => sum + zone.discovered, 0);
  return <><PageIntro eyebrow="05 / monitoring reach" title="Coverage, zone by zone." detail="Coverage is a confidence boundary. See where Guardian has a steady view, where discovery is outpacing monitoring, and where to focus next." /><div className="mb-4 grid gap-4 sm:grid-cols-3"><StatCard label="Zones in view" value={zones.length} detail="Authorized network ranges" accent="teal" icon={Layers3} /><StatCard label="Monitored endpoints" value={totalMonitored} detail={`${totalDiscovered} discovered total`} accent="teal" icon={Network} /><StatCard label="Average coverage" value={`${zones.length ? Math.round(zones.reduce((sum, zone) => sum + zone.coverage, 0) / zones.length) : 0}%`} detail="Across active zones" accent="yellow" icon={Target} /></div>{query.isLoading ? <LoadingState label="Loading coverage" /> : query.isError ? <ErrorState retry={() => query.refetch()} /> : zones.length ? <div className="grid gap-4 lg:grid-cols-2">{zones.map((zone) => <ZoneCard key={zone.name} zone={zone} />)}</div> : <EmptyState title="No coverage zones" detail="Add an authorized asset or run an observation to establish network coverage." />}</>;
}

function ZoneCard({ zone }: { zone: CoverageZone }) {
  return <Panel className="p-5" ><div className="flex items-start justify-between gap-3"><div><div className="flex items-center gap-2"><Globe2 size={17} className="text-[#238a85]" /><h2 className="font-[var(--app-font-serif)] text-xl font-bold">{zone.name}</h2></div><div className="mt-2 font-mono text-[10px] uppercase tracking-[0.12em] text-[#89958f]">{zone.range}</div></div><span className={cx("border px-2 py-1 font-mono text-[9px] uppercase tracking-[0.1em]", statusTone(zone.state))}>{zone.state}</span></div><div className="mt-7 flex items-end justify-between"><div><span className="font-[var(--app-font-serif)] text-5xl font-bold tracking-[-0.08em]">{zone.coverage}</span><span className="ml-1 font-mono text-sm text-[#73827e]">%</span></div><div className="text-right font-mono text-[10px] uppercase leading-5 tracking-[0.1em] text-[#82908b]">{zone.monitored} monitored<br />{zone.discovered} discovered</div></div><div className="mt-4 h-3 bg-[#dee7df]"><div className={cx("h-full", zone.coverage >= 85 ? "bg-[#238a85]" : zone.coverage >= 60 ? "bg-[#f2c94c]" : "bg-[#ef6c52]")} style={{ width: `${zone.coverage}%` }} /></div><div className="mt-4 flex items-center justify-between border-t border-[#ded9ce] pt-4 text-xs text-[#75837f]"><span>{zone.discovered - zone.monitored > 0 ? `${zone.discovered - zone.monitored} endpoints need scope` : "All discovered endpoints monitored"}</span><span className="font-mono text-[10px] text-[#238a85]">{zone.coverage >= 85 ? "strong view" : "review boundary"}</span></div></Panel>;
}
