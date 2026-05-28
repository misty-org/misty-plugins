import fs from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, "..");
const metadataDir = path.join(__dirname, "plugins");
const pageSize = 3;
const artifactBaseUrl =
  "https://github.com/misty-org/misty-hub/releases/download/plugins";

function hasTemplateValue(value) {
  return typeof value === "string" && value.includes("@");
}

function resolveField(primary, fallback) {
  if (primary === undefined || primary === null) {
    return fallback;
  }
  if (hasTemplateValue(primary)) {
    return fallback ?? primary;
  }
  return primary;
}

async function readJson(jsonPath) {
  return JSON.parse(await fs.readFile(jsonPath, "utf8"));
}

async function loadCatalogSource(metadataPath) {
  const metadata = await readJson(metadataPath);
  const pluginDir = metadata.plugin_dir ?? metadata.manifest?.id ?? path.basename(metadataPath, ".json");
  const manifestPath = path.join(repoRoot, "plugins", pluginDir, "manifest.json");

  let manifest = metadata.manifest ?? null;
  if (!manifest) {
    manifest = await readJson(manifestPath);
  }

  return { metadata, manifest };
}

function buildCatalogEntry(manifest, metadata) {
  const version = resolveField(manifest.version, metadata.version);
  const name = resolveField(manifest.name, metadata.name);
  const author = resolveField(manifest.author, metadata.author);
  const description = resolveField(manifest.description, metadata.description);
  const id = resolveField(manifest.id, metadata.id);

  if ([id, name, version, author].some((value) => !value || hasTemplateValue(value))) {
    throw new Error(`Catalog entry ${metadata.id ?? id ?? "unknown"} has unresolved core fields.`);
  }

  return {
    id,
    name,
    version,
    author,
    overview: metadata.overview ?? description ?? "",
    status: metadata.status ?? "available",
    capabilities: metadata.capabilities ?? [],
    where_it_appears: metadata.where_it_appears ?? [],
    permissions: metadata.permissions ?? [],
    getting_started: metadata.getting_started ?? [],
    changelog: metadata.changelog ?? [],
    links: metadata.links ?? [],
    actions: metadata.actions ?? [],
    verified: metadata.verified ?? false,
    launcher: metadata.launcher ?? {
      views: [],
      show_in_launcher: true,
      requires_selected_file: false,
      open_mode: "tab",
    },
    install: {
      root: metadata.install.root,
      artifacts: (metadata.install.platforms ?? []).map((platform) => ({
        platform,
        url: `${artifactBaseUrl}/${metadata.install.artifact_base_name}-${platform}.zip`,
      })),
    },
  };
}

function byCatalogOrder(left, right) {
  const leftRank = left.featured_rank ?? Number.MAX_SAFE_INTEGER;
  const rightRank = right.featured_rank ?? Number.MAX_SAFE_INTEGER;
  if (leftRank !== rightRank) {
    return leftRank - rightRank;
  }
  return left.entry.name.localeCompare(right.entry.name);
}

async function main() {
  const metadataFiles = (await fs.readdir(metadataDir))
    .filter((file) => file.endsWith(".json"))
    .sort();

  const catalogSources = [];
  for (const file of metadataFiles) {
    const { metadata, manifest } = await loadCatalogSource(path.join(metadataDir, file));
    const entry = buildCatalogEntry(manifest, metadata);
    catalogSources.push({
      featured_rank: metadata.featured_rank,
      published: metadata.published ?? true,
      entry,
    });
  }

  const publishedEntries = catalogSources.filter((source) => source.published).sort(byCatalogOrder);
  const totalPages = Math.max(1, Math.ceil(publishedEntries.length / pageSize));
  const generatedAt = new Date().toISOString();

  const index = {
    schema_version: 1,
    generated_at: generatedAt,
    page_size: pageSize,
    total_plugins: publishedEntries.length,
    total_pages: totalPages,
    plugins: [],
  };

  for (let page = 1; page <= totalPages; page += 1) {
    const start = (page - 1) * pageSize;
    const pageEntries = publishedEntries.slice(start, start + pageSize).map((source) => source.entry);
    const pagePayload = {
      schema_version: 1,
      generated_at: generatedAt,
      page,
      page_size: pageSize,
      total_plugins: publishedEntries.length,
      total_pages: totalPages,
      plugins: pageEntries,
    };

    index.plugins.push(
      ...pageEntries.map((plugin) => ({
        id: plugin.id,
        name: plugin.name,
        version: plugin.version,
        author: plugin.author,
        overview: plugin.overview,
        status: plugin.status,
        verified: plugin.verified,
        page,
      })),
    );

    await fs.writeFile(
      path.join(__dirname, `page-${page}.json`),
      `${JSON.stringify(pagePayload, null, 2)}\n`,
    );
  }

  await fs.writeFile(
    path.join(__dirname, "index.json"),
    `${JSON.stringify(index, null, 2)}\n`,
  );
}

await main();
