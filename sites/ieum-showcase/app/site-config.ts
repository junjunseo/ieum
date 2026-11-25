export const site = {
  github: "https://github.com/junjunseo/ieum",
  youtubeId: "69tobLw-lX4",
};

export function siteAsset(path: `/${string}`) {
  return `${process.env.NEXT_PUBLIC_IEUM_BASE_PATH ?? ""}${path}`;
}
