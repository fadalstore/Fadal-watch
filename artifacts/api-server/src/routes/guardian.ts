import net from "node:net";
import { and, asc, count, desc, eq, ilike, or } from "drizzle-orm";
import { Router, type IRouter } from "express";
import {
  CreateAssetBody,
  CreateScanRunBody,
  GetAssetParams,
  ListActivityQueryParams,
  ListAlertsQueryParams,
  ListAssetsQueryParams,
  UpdateAlertBody,
  UpdateAlertParams,
} from "@workspace/api-zod";
import {
  db,
  guardianActivity,
  guardianAlerts,
  guardianAssets,
  guardianCoverageZones,
  guardianScanRuns,
  guardianServices,
} from "@workspace/db";

const router: IRouter = Router();
let seedPromise: Promise<void> | null = null;

const now = () => new Date();
const iso = (value: Date | string | null | undefined) =>
  value instanceof Date ? value.toISOString() : value ?? null;
const id = (prefix: string) => `${prefix}-${crypto.randomUUID().slice(0, 8)}`;

async function seedGuardian() {
  const existing = await db
    .select({ total: count() })
    .from(guardianAssets);
  if (Number(existing[0]?.total ?? 0) > 0) return;

  const current = now();
  const firstSeen = new Date(current.getTime() - 1000 * 60 * 60 * 24 * 18);
  const assets = [
    {
      id: "asset-edge-router",
      address: "10.10.0.1",
      label: "Edge Router",
      type: "network",
      status: "healthy",
      exposure: 18,
      risk: 24,
      firstSeen,
      lastSeen: new Date(current.getTime() - 1000 * 60 * 7),
      services: 4,
      baselineState: "stable",
      zone: "Core network",
    },
    {
      id: "asset-billing-api",
      address: "10.10.10.12",
      label: "Billing API",
      type: "server",
      status: "watch",
      exposure: 52,
      risk: 61,
      firstSeen: new Date(current.getTime() - 1000 * 60 * 60 * 24 * 12),
      lastSeen: new Date(current.getTime() - 1000 * 60 * 13),
      services: 6,
      baselineState: "drift",
      zone: "Production",
    },
    {
      id: "asset-design-studio",
      address: "10.10.20.44",
      label: "Design Studio",
      type: "workstation",
      status: "healthy",
      exposure: 28,
      risk: 22,
      firstSeen: new Date(current.getTime() - 1000 * 60 * 60 * 24 * 6),
      lastSeen: new Date(current.getTime() - 1000 * 60 * 4),
      services: 2,
      baselineState: "stable",
      zone: "Studio",
    },
    {
      id: "asset-backup-node",
      address: "10.10.10.21",
      label: "Backup Node",
      type: "server",
      status: "critical",
      exposure: 74,
      risk: 82,
      firstSeen: new Date(current.getTime() - 1000 * 60 * 60 * 24 * 23),
      lastSeen: new Date(current.getTime() - 1000 * 60 * 25),
      services: 5,
      baselineState: "learning",
      zone: "Production",
    },
  ];
  await db.insert(guardianAssets).values(assets).onConflictDoNothing();

  await db
    .insert(guardianServices)
    .values([
      { id: "svc-edge-https", assetId: "asset-edge-router", name: "HTTPS", port: 443, state: "expected", lastObserved: current, note: "Expected management surface" },
      { id: "svc-edge-dns", assetId: "asset-edge-router", name: "DNS", port: 53, state: "expected", lastObserved: current, note: "Internal resolver" },
      { id: "svc-billing-https", assetId: "asset-billing-api", name: "HTTPS", port: 443, state: "expected", lastObserved: current, note: "Primary API surface" },
      { id: "svc-billing-admin", assetId: "asset-billing-api", name: "Admin console", port: 8443, state: "new", lastObserved: current, note: "First observed 14 minutes ago" },
      { id: "svc-billing-db", assetId: "asset-billing-api", name: "PostgreSQL", port: 5432, state: "risky", lastObserved: current, note: "Review exposure policy" },
      { id: "svc-studio-ssh", assetId: "asset-design-studio", name: "SSH", port: 22, state: "expected", lastObserved: current, note: "Known maintenance channel" },
      { id: "svc-backup-smb", assetId: "asset-backup-node", name: "SMB", port: 445, state: "risky", lastObserved: current, note: "Segment from untrusted zones" },
      { id: "svc-backup-ssh", assetId: "asset-backup-node", name: "SSH", port: 22, state: "expected", lastObserved: current, note: "Known maintenance channel" },
    ])
    .onConflictDoNothing();

  await db
    .insert(guardianAlerts)
    .values([
      {
        id: "alert-billing-admin",
        title: "New admin surface observed",
        reason: "Billing API exposed port 8443 outside its learned baseline.",
        severity: "high",
        state: "open",
        assetId: "asset-billing-api",
        createdAt: new Date(current.getTime() - 1000 * 60 * 14),
        recommendation: "Confirm the deployment change, then restrict the service to its expected zone.",
        evidence: ["Port 8443 was not present in the last 12 observations", "Asset is in the Production zone"],
      },
      {
        id: "alert-backup-smb",
        title: "Backup node has risky exposure",
        reason: "SMB is reachable on a critical backup asset.",
        severity: "critical",
        state: "open",
        assetId: "asset-backup-node",
        createdAt: new Date(current.getTime() - 1000 * 60 * 52),
        recommendation: "Review segmentation and firewall policy before the next backup window.",
        evidence: ["Port 445 is marked risky", "Asset risk score is 82/100"],
      },
      {
        id: "alert-edge-baseline",
        title: "Baseline is healthy",
        reason: "Edge Router is stable across recent observations.",
        severity: "low",
        state: "acknowledged",
        assetId: "asset-edge-router",
        createdAt: new Date(current.getTime() - 1000 * 60 * 60 * 4),
        recommendation: "No action required. Keep observing the expected management surface.",
        evidence: ["No new services in the last 18 observations"],
      },
    ])
    .onConflictDoNothing();

  await db
    .insert(guardianActivity)
    .values([
      { id: "activity-billing-admin", kind: "alert", title: "New admin surface observed", detail: "Billing API added port 8443 to its observed service set.", timestamp: new Date(current.getTime() - 1000 * 60 * 14), severity: "high", assetId: "asset-billing-api" },
      { id: "activity-scan-1", kind: "scan", title: "Observation run completed", detail: "Production and Core network were checked with 98% response coverage.", timestamp: new Date(current.getTime() - 1000 * 60 * 31), severity: "info", assetId: null },
      { id: "activity-backup-smb", kind: "baseline", title: "Baseline drift detected", detail: "Backup Node has a risky service outside its expected exposure profile.", timestamp: new Date(current.getTime() - 1000 * 60 * 52), severity: "critical", assetId: "asset-backup-node" },
      { id: "activity-coverage", kind: "response", title: "Coverage improved", detail: "Studio zone moved from learning to stable after three consistent observations.", timestamp: new Date(current.getTime() - 1000 * 60 * 60 * 3), severity: "info", assetId: null },
      { id: "activity-baseline", kind: "baseline", title: "Core network baseline confirmed", detail: "18 observations agree on the current expected service set.", timestamp: new Date(current.getTime() - 1000 * 60 * 60 * 5), severity: "info", assetId: "asset-edge-router" },
    ])
    .onConflictDoNothing();

  await db
    .insert(guardianScanRuns)
    .values({
      id: "scan-initial",
      scope: "10.10.0.0/16",
      mode: "observation",
      status: "completed",
      startedAt: new Date(current.getTime() - 1000 * 60 * 31),
      assetsObserved: 4,
      changesFound: 2,
      message: "Observation complete. Two changes need review.",
    })
    .onConflictDoNothing();

  await db
    .insert(guardianCoverageZones)
    .values([
      { id: "zone-core", name: "Core network", range: "10.10.0.0/24", monitored: 18, discovered: 18, coverage: 100, state: "healthy" },
      { id: "zone-production", name: "Production", range: "10.10.10.0/24", monitored: 31, discovered: 34, coverage: 91, state: "partial" },
      { id: "zone-studio", name: "Studio", range: "10.10.20.0/24", monitored: 12, discovered: 12, coverage: 100, state: "healthy" },
    ])
    .onConflictDoNothing();
}

async function ensureSeeded() {
  if (!seedPromise) {
    seedPromise = seedGuardian().catch((error) => {
      seedPromise = null;
      throw error;
    });
  }
  await seedPromise;
}

function mapAsset(asset: typeof guardianAssets.$inferSelect) {
  return {
    id: asset.id,
    address: asset.address,
    label: asset.label,
    type: asset.type,
    status: asset.status,
    exposure: asset.exposure,
    risk: asset.risk,
    lastSeen: asset.lastSeen.toISOString(),
    firstSeen: asset.firstSeen.toISOString(),
    services: asset.services,
    baselineState: asset.baselineState,
  };
}

function mapActivity(item: typeof guardianActivity.$inferSelect) {
  return {
    id: item.id,
    kind: item.kind,
    title: item.title,
    detail: item.detail,
    timestamp: item.timestamp.toISOString(),
    severity: item.severity,
    assetId: item.assetId,
  };
}

function mapAlert(item: typeof guardianAlerts.$inferSelect) {
  return {
    id: item.id,
    title: item.title,
    reason: item.reason,
    severity: item.severity,
    state: item.state,
    assetId: item.assetId,
    createdAt: item.createdAt.toISOString(),
    recommendation: item.recommendation,
    evidence: item.evidence ?? [],
  };
}

function ipv4ToNumber(value: string) {
  return value.split(".").reduce((result, octet) => result * 256 + Number(octet), 0);
}

function numberToIpv4(value: number) {
  return [24, 16, 8, 0].map((shift) => (value >>> shift) & 255).join(".");
}

function parseObservationScope(scope: string) {
  const value = scope.trim();
  if (net.isIP(value) === 4) return [value];
  const match = value.match(/^(\d{1,3}(?:\.\d{1,3}){3})\/(\d{1,2})$/);
  if (!match || net.isIP(match[1]) !== 4) return null;
  const prefix = Number(match[2]);
  if (prefix < 16 || prefix > 32) return null;
  const base = ipv4ToNumber(match[1]) & (0xffffffff << (32 - prefix));
  const size = 2 ** (32 - prefix);
  const first = prefix >= 31 ? 0 : 1;
  const last = prefix >= 31 ? size : size - 1;
  if (size - (prefix >= 31 ? 0 : 2) > 64) return null;
  return Array.from({ length: Math.max(1, last - first) }, (_, index) => numberToIpv4(base + first + index));
}

function probePort(address: string, port: number, timeout = 350) {
  return new Promise<boolean>((resolve) => {
    const socket = net.createConnection({ host: address, port });
    const timer = setTimeout(() => {
      socket.destroy();
      resolve(false);
    }, timeout);
    const finish = (open: boolean) => {
      clearTimeout(timer);
      socket.destroy();
      resolve(open);
    };
    socket.once("connect", () => finish(true));
    socket.once("error", () => finish(false));
    socket.once("timeout", () => finish(false));
    socket.setTimeout(timeout);
  });
}

async function observeAddresses(addresses: string[]) {
  const ports = [22, 53, 80, 443, 445, 5432, 8080, 8443];
  const observations: Array<{ address: string; ports: number[] }> = [];
  for (let offset = 0; offset < addresses.length; offset += 8) {
    const batch = addresses.slice(offset, offset + 8);
    const results = await Promise.all(
      batch.map(async (address) => {
        const states = await Promise.all(ports.map(async (port) => ({ port, open: await probePort(address, port) })));
        return { address, ports: states.filter((item) => item.open).map((item) => item.port) };
      }),
    );
    observations.push(...results.filter((item) => item.ports.length > 0));
  }
  return observations;
}

function serviceName(port: number) {
  return ({ 22: "SSH", 53: "DNS", 80: "HTTP", 443: "HTTPS", 445: "SMB", 5432: "PostgreSQL", 8080: "HTTP alternate", 8443: "HTTPS alternate" } as Record<number, string>)[port] ?? `TCP ${port}`;
}

router.get("/overview", async (_req, res) => {
  await ensureSeeded();
  const assets = await db.select().from(guardianAssets);
  const alerts = await db.select().from(guardianAlerts);
  const activity = await db.select().from(guardianActivity).orderBy(desc(guardianActivity.timestamp)).limit(6);
  const avgRisk = assets.length
    ? assets.reduce((total, asset) => total + asset.risk, 0) / assets.length
    : 0;
  const recentScan = await db.select().from(guardianScanRuns).orderBy(desc(guardianScanRuns.startedAt)).limit(1);
  const counts = assets.reduce<Record<string, number>>((result, asset) => {
    result[asset.type] = (result[asset.type] ?? 0) + 1;
    return result;
  }, {});

  res.json({
    healthScore: Math.max(0, Math.round(100 - avgRisk)),
    monitoredAssets: assets.length,
    activeAlerts: alerts.filter((alert) => alert.state !== "resolved").length,
    newToday: activity.filter((item) => item.kind === "alert").length,
    exposedServices: assets.filter((asset) => asset.exposure >= 50).reduce((total, asset) => total + asset.services, 0),
    coverage: 97,
    lastScan: iso(recentScan[0]?.startedAt),
    assetsByType: Object.entries(counts).map(([type, count]) => ({ type, count })),
    trend: [
      { label: "6d", score: 88 },
      { label: "5d", score: 89 },
      { label: "4d", score: 91 },
      { label: "3d", score: 90 },
      { label: "2d", score: 93 },
      { label: "1d", score: 92 },
      { label: "Now", score: Math.max(0, Math.round(100 - avgRisk)) },
    ],
    recentActivity: activity.map(mapActivity),
  });
});

router.get("/assets", async (req, res) => {
  await ensureSeeded();
  const params = ListAssetsQueryParams.parse({
    status: req.query.status ?? "all",
    search: req.query.search,
  });
  const filters = [];
  if (params.status !== "all") filters.push(eq(guardianAssets.status, params.status));
  if (params.search) {
    filters.push(or(ilike(guardianAssets.label, `%${params.search}%`), ilike(guardianAssets.address, `%${params.search}%`)));
  }
  const assets = await db.select().from(guardianAssets).where(filters.length ? and(...filters) : undefined).orderBy(asc(guardianAssets.label));
  res.json(assets.map(mapAsset));
});

router.post("/assets", async (req, res) => {
  await ensureSeeded();
  const body = CreateAssetBody.parse(req.body);
  const current = now();
  const asset = {
    id: id("asset"),
    address: body.address,
    label: body.label,
    type: body.type,
    status: "healthy",
    exposure: 0,
    risk: 0,
    firstSeen: current,
    lastSeen: current,
    services: 0,
    baselineState: "learning",
    zone: body.zone ?? "Unassigned",
  };
  await db.insert(guardianAssets).values(asset);
  await db.insert(guardianActivity).values({
    id: id("activity"),
    kind: "asset",
    title: "Asset added to monitoring",
    detail: `${body.label} is ready for its first observation.`,
    timestamp: current,
    severity: "info",
    assetId: asset.id,
  });
  res.status(201).json(mapAsset(asset));
});

router.get("/assets/:assetId", async (req, res) => {
  await ensureSeeded();
  const { assetId } = GetAssetParams.parse(req.params);
  const rows = await db.select().from(guardianAssets).where(eq(guardianAssets.id, assetId)).limit(1);
  if (!rows[0]) {
    res.status(404).json({ error: "Asset not found" });
    return;
  }
  const services = await db.select().from(guardianServices).where(eq(guardianServices.assetId, assetId)).orderBy(asc(guardianServices.port));
  const history = await db.select().from(guardianActivity).where(eq(guardianActivity.assetId, assetId)).orderBy(desc(guardianActivity.timestamp)).limit(12);
  res.json({
    ...mapAsset(rows[0]),
    zone: rows[0].zone,
    servicesList: services.map((service) => ({
      name: service.name,
      port: service.port,
      state: service.state,
      lastObserved: service.lastObserved.toISOString(),
      note: service.note,
    })),
    history: history.map(mapActivity),
  });
});

router.post("/scan-runs", async (req, res) => {
  await ensureSeeded();
  const body = CreateScanRunBody.parse(req.body);
  const current = now();
  if (!body.authorizationConfirmed) {
    res.status(201).json({
      id: id("scan"),
      scope: body.scope,
      mode: body.mode,
      status: "rejected",
      startedAt: current.toISOString(),
      assetsObserved: 0,
      changesFound: 0,
      message: "Authorization must be confirmed before an observation run can start.",
    });
    return;
  }
  const addresses = parseObservationScope(body.scope);
  if (!addresses) {
    res.status(400).json({ error: "Use a single IPv4 address or an IPv4 CIDR between /16 and /32 with at most 64 host addresses." });
    return;
  }
  const observations = await observeAddresses(addresses);
  const assets = await db.select().from(guardianAssets);
  const openAlerts = await db.select().from(guardianAlerts).where(eq(guardianAlerts.state, "open"));
  const knownByAddress = new Map(assets.map((asset) => [asset.address, asset]));
  for (const observation of observations) {
    const existing = knownByAddress.get(observation.address);
    if (existing) {
      await db.update(guardianAssets).set({
        lastSeen: current,
        services: observation.ports.length,
        baselineState: existing.baselineState === "learning" ? "learning" : existing.baselineState,
      }).where(eq(guardianAssets.id, existing.id));
      continue;
    }
    const observedAsset = {
      id: id("asset"),
      address: observation.address,
      label: `Observed ${observation.address}`,
      type: "unknown",
      status: "healthy",
      exposure: 0,
      risk: 0,
      firstSeen: current,
      lastSeen: current,
      services: observation.ports.length,
      baselineState: "learning",
      zone: "New observation",
    };
    await db.insert(guardianAssets).values(observedAsset);
    await db.insert(guardianServices).values(observation.ports.map((port) => ({
      id: id("svc"),
      assetId: observedAsset.id,
      name: serviceName(port),
      port,
      state: "new",
      lastObserved: current,
      note: "Observed during an authorized run",
    })));
    await db.insert(guardianActivity).values({
      id: id("activity"),
      kind: "asset",
      title: "New asset observed",
      detail: `${observation.address} responded on ${observation.ports.length} selected service port(s).`,
      timestamp: current,
      severity: "medium",
      assetId: observedAsset.id,
    });
  }
  const scan = {
    id: id("scan"),
    scope: body.scope,
    mode: body.mode,
    status: "completed",
    startedAt: current,
    assetsObserved: observations.length,
    changesFound: openAlerts.length,
    message: observations.length
      ? `${observations.length} responsive asset(s) found across ${addresses.length} authorized address(es).`
      : "No selected service ports responded in the authorized scope.",
  };
  await db.insert(guardianScanRuns).values(scan);
  await db.insert(guardianActivity).values({
    id: id("activity"),
    kind: "scan",
    title: "Observation run completed",
    detail: `${scan.assetsObserved} assets observed in ${body.scope}.`,
    timestamp: current,
    severity: openAlerts.length ? "medium" : "info",
    assetId: null,
  });
  res.status(201).json({ ...scan, startedAt: current.toISOString() });
});

router.get("/activity", async (req, res) => {
  await ensureSeeded();
  const params = ListActivityQueryParams.parse({ limit: req.query.limit });
  const activity = await db.select().from(guardianActivity).orderBy(desc(guardianActivity.timestamp)).limit(params.limit);
  res.json(activity.map(mapActivity));
});

router.get("/alerts", async (req, res) => {
  await ensureSeeded();
  const params = ListAlertsQueryParams.parse({ state: req.query.state ?? "all" });
  const alerts = await db.select().from(guardianAlerts)
    .where(params.state === "all" ? undefined : eq(guardianAlerts.state, params.state))
    .orderBy(desc(guardianAlerts.createdAt));
  res.json(alerts.map(mapAlert));
});

router.patch("/alerts/:alertId", async (req, res) => {
  await ensureSeeded();
  const { alertId } = UpdateAlertParams.parse(req.params);
  const { state } = UpdateAlertBody.parse(req.body);
  const updated = await db.update(guardianAlerts).set({ state }).where(eq(guardianAlerts.id, alertId)).returning();
  if (!updated[0]) {
    res.status(404).json({ error: "Alert not found" });
    return;
  }
  await db.insert(guardianActivity).values({
    id: id("activity"),
    kind: "response",
    title: `Alert ${state}`,
    detail: updated[0].title,
    timestamp: now(),
    severity: updated[0].severity,
    assetId: updated[0].assetId,
  });
  res.json(mapAlert(updated[0]));
});

router.get("/coverage", async (_req, res) => {
  await ensureSeeded();
  const zones = await db.select().from(guardianCoverageZones).orderBy(asc(guardianCoverageZones.name));
  res.json(zones);
});

export default router;