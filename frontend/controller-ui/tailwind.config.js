/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        console: {
          bg: "#080c10",
          panel: "#0d1318",
          panel2: "#121a20",
          border: "#1e3428",
          border2: "#254634",
          text: "#c8e8d0",
          muted: "#7aaa90",
          dim: "#3a6050",
          green: "#00ff88",
          cyan: "#00e5ff",
          red: "#ff3355",
          amber: "#ffaa00",
          purple: "#7c6fff",
        },
      },
      fontFamily: {
        mono: ['"Share Tech Mono"', "ui-monospace", "SFMono-Regular", "monospace"],
        sans: ["Rajdhani", "Inter", "ui-sans-serif", "system-ui", "sans-serif"],
      },
      boxShadow: {
        console: "0 0 0 1px rgba(0,255,136,0.08), 0 18px 45px rgba(0,0,0,0.38)",
        glow: "0 0 22px rgba(0,255,136,0.18)",
        cyanGlow: "0 0 22px rgba(0,229,255,0.15)",
      },
      animation: {
        pulseDot: "pulseDot 1.8s ease-in-out infinite",
        scanline: "scanline 6s linear infinite",
        softIn: "softIn 260ms ease-out both",
      },
      keyframes: {
        pulseDot: {
          "0%, 100%": { opacity: "1", transform: "scale(1)" },
          "50%": { opacity: "0.45", transform: "scale(0.82)" },
        },
        scanline: {
          "0%": { transform: "translateY(-100%)" },
          "100%": { transform: "translateY(100%)" },
        },
        softIn: {
          "0%": { opacity: "0", transform: "translateY(8px)" },
          "100%": { opacity: "1", transform: "translateY(0)" },
        },
      },
    },
  },
  plugins: [],
};