import type { NextConfig } from "next";

const basePath = process.env.NEXT_PUBLIC_IEUM_BASE_PATH;

const nextConfig: NextConfig = basePath ? {
  basePath,
  output: "export",
  trailingSlash: true,
} : {};

export default nextConfig;
