import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "이음 IEUM — 설계 규칙을 언어에 담다",
  description: "모듈의 의존 관계와 계층을 선언하고, 구조적 위반을 실행 전에 검사하는 프로그래밍 언어 이음. 실제 검사 결과를 통해 연결의 규칙을 살펴보세요.",
  icons: {
    icon: "/favicon.svg",
    shortcut: "/favicon.svg",
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="ko">
      <body>{children}</body>
    </html>
  );
}
