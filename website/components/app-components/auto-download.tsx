"use client";

import { useEffect, useState } from "react";
import { FiDownload } from "react-icons/fi";
import { FaWindows, FaLinux, FaApple } from "react-icons/fa6";
import { Button } from "@/components/ui";

interface PlatformInfo {
  os: "windows" | "linux" | "macos" | "unknown";
  osName: string;
  arch: string;
  fileName: string;
  downloadUrl: string;
}

export function AutoDownloadButton({
  variant = "default",
  size = "lg",
  className = "",
}: {
  variant?: "default" | "outline" | "secondary";
  size?: "default" | "sm" | "lg";
  className?: string;
}) {
  const [platform, setPlatform] = useState<PlatformInfo>({
    os: "windows",
    osName: "Windows",
    arch: "x86_64",
    fileName: "minigit.exe",
    downloadUrl: "/assets/minigit.exe",
  });

  useEffect(() => {
    if (typeof window === "undefined" || !navigator) return;

    const userAgent = navigator.userAgent.toLowerCase();
    const platformStr = (navigator as unknown as { userAgentData?: { platform?: string } })
      ?.userAgentData?.platform?.toLowerCase() ||
      navigator.platform?.toLowerCase() ||
      userAgent;

    let os: "windows" | "linux" | "macos" | "unknown" = "unknown";
    let osName = "Universal";
    let arch = "64-bit";
    let fileName = "minigit.exe";
    let downloadUrl = "/assets/minigit.exe";

    if (/win/i.test(platformStr) || /windows/i.test(userAgent)) {
      os = "windows";
      osName = "Windows";
      arch = "x86_64";
      fileName = "minigit.exe";
      downloadUrl = "/assets/minigit.exe";
    } else if (/mac/i.test(platformStr) || /macintosh/i.test(userAgent)) {
      os = "macos";
      osName = "macOS";
      arch = "Apple Silicon (arm64)";
      fileName = "minigit-macos";
      downloadUrl = "/assets/minigit-macos";
    } else if (/linux/i.test(platformStr) || /linux/i.test(userAgent)) {
      os = "linux";
      osName = "Linux";
      arch = "x86_64";
      fileName = "minigit-linux";
      downloadUrl = "/assets/minigit-linux";
    }

    setPlatform({ os, osName, arch, fileName, downloadUrl });
  }, []);

  const renderIcon = () => {
    switch (platform.os) {
      case "windows":
        return <FaWindows className="mr-2 h-4 w-4" />;
      case "macos":
        return <FaApple className="mr-2 h-4 w-4" />;
      case "linux":
        return <FaLinux className="mr-2 h-4 w-4" />;
      default:
        return <FiDownload className="mr-2 h-4 w-4" />;
    }
  };

  return (
    <Button
      variant={variant}
      size={size}
      asChild
      className={`bg-primary text-primary-foreground hover:bg-primary/90 font-medium shrink-0 shadow-sm cursor-pointer ${className}`}
    >
      <a href={platform.downloadUrl} download={platform.fileName}>
        {renderIcon()}
        <span>
          Download ({platform.fileName})
        </span>
      </a>
    </Button>
  );
}

export function PlatformDetectorCard() {
  const [platform, setPlatform] = useState<PlatformInfo>({
    os: "windows",
    osName: "Windows",
    arch: "x86_64",
    fileName: "minigit.exe",
    downloadUrl: "/assets/minigit.exe",
  });

  useEffect(() => {
    if (typeof window === "undefined" || !navigator) return;

    const userAgent = navigator.userAgent.toLowerCase();
    const platformStr = (navigator as unknown as { userAgentData?: { platform?: string } })
      ?.userAgentData?.platform?.toLowerCase() ||
      navigator.platform?.toLowerCase() ||
      userAgent;

    let os: "windows" | "linux" | "macos" | "unknown" = "windows";
    let osName = "Windows";
    let arch = "x86_64";
    let fileName = "minigit.exe";
    let downloadUrl = "/assets/minigit.exe";

    if (/mac/i.test(platformStr) || /macintosh/i.test(userAgent)) {
      os = "macos";
      osName = "macOS";
      arch = "Apple Silicon (arm64)";
      fileName = "minigit-macos";
      downloadUrl = "/assets/minigit-macos";
    } else if (/linux/i.test(platformStr) || /linux/i.test(userAgent)) {
      os = "linux";
      osName = "Linux";
      arch = "x86_64";
      fileName = "minigit-linux";
      downloadUrl = "/assets/minigit-linux";
    }

    setPlatform({ os, osName, arch, fileName, downloadUrl });
  }, []);

  return (
    <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <div className="flex items-center gap-2">
          <h2 className="font-semibold text-base text-foreground">
            Download for {platform.osName}
          </h2>
          <span className="px-2 py-0.5 rounded-full text-[10px] font-mono bg-emerald-500/10 text-emerald-700 dark:text-emerald-400 border border-emerald-500/20">
            Detected: {platform.arch}
          </span>
        </div>
        <p className="text-xs text-muted-foreground mt-0.5">
          Native standalone pre-compiled executable ({platform.fileName}) • No dependencies needed
        </p>
      </div>

      <AutoDownloadButton />
    </div>
  );
}
