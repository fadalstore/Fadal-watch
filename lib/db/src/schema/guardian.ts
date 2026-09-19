import { sql } from "drizzle-orm";
import {
  integer,
  jsonb,
  pgTable,
  text,
  timestamp,
} from "drizzle-orm/pg-core";

export const guardianAssets = pgTable("guardian_assets", {
  id: text("id").primaryKey(),
  address: text("address").notNull().unique(),
  label: text("label").notNull(),
  type: text("type").notNull(),
  status: text("status").notNull(),
  exposure: integer("exposure").notNull().default(0),
  risk: integer("risk").notNull().default(0),
  firstSeen: timestamp("first_seen", { withTimezone: true }).notNull(),
  lastSeen: timestamp("last_seen", { withTimezone: true }).notNull(),
  services: integer("services").notNull().default(0),
  baselineState: text("baseline_state").notNull().default("learning"),
  zone: text("zone").notNull().default("Unassigned"),
});

export const guardianServices = pgTable("guardian_services", {
  id: text("id").primaryKey(),
  assetId: text("asset_id").notNull(),
  name: text("name").notNull(),
  port: integer("port").notNull(),
  state: text("state").notNull(),
  lastObserved: timestamp("last_observed", { withTimezone: true }).notNull(),
  note: text("note"),
});

export const guardianAlerts = pgTable("guardian_alerts", {
  id: text("id").primaryKey(),
  title: text("title").notNull(),
  reason: text("reason").notNull(),
  severity: text("severity").notNull(),
  state: text("state").notNull().default("open"),
  assetId: text("asset_id").notNull(),
  createdAt: timestamp("created_at", { withTimezone: true }).notNull(),
  recommendation: text("recommendation").notNull(),
  evidence: jsonb("evidence").$type<string[]>().notNull().default(sql`'[]'::jsonb`),
});

export const guardianActivity = pgTable("guardian_activity", {
  id: text("id").primaryKey(),
  kind: text("kind").notNull(),
  title: text("title").notNull(),
  detail: text("detail").notNull(),
  timestamp: timestamp("timestamp", { withTimezone: true }).notNull(),
  severity: text("severity").notNull(),
  assetId: text("asset_id"),
});

export const guardianScanRuns = pgTable("guardian_scan_runs", {
  id: text("id").primaryKey(),
  scope: text("scope").notNull(),
  mode: text("mode").notNull(),
  status: text("status").notNull(),
  startedAt: timestamp("started_at", { withTimezone: true }).notNull(),
  assetsObserved: integer("assets_observed").notNull().default(0),
  changesFound: integer("changes_found").notNull().default(0),
  message: text("message"),
});

export const guardianCoverageZones = pgTable("guardian_coverage_zones", {
  id: text("id").primaryKey(),
  name: text("name").notNull(),
  range: text("range").notNull(),
  monitored: integer("monitored").notNull().default(0),
  discovered: integer("discovered").notNull().default(0),
  coverage: integer("coverage").notNull().default(0),
  state: text("state").notNull(),
});