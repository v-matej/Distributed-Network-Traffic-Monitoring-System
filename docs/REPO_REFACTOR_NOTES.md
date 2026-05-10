# Repository Refactor Notes

Applied structural refactor:

- backend code is grouped by module:
  - `backend/agent/`
  - `backend/controller/`
  - `backend/sniffer/`
- app entrypoints moved to:
  - `backend/apps/agent_server/`
  - `backend/apps/controller_server/`
  - `backend/apps/packet_sniffer/`
- shared third-party headers remain in `backend/external/`
- runtime directories live under:
  - `backend/data/`
  - `backend/captures/`
- root `Makefile` orchestrates both backend and frontend workflows

Next frontend scaffold command:

```bash
npm create vite@latest frontend/controller-ui -- --template react-ts
```
