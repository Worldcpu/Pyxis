// 根路由（决策 32：模块路由；飞行计划激活 + 航图/设置占位）。

import { createBrowserRouter, RouterProvider } from 'react-router-dom';

import { AppShell } from './components/app-shell';
import { ComingSoonPage } from './pages/coming-soon';
import { FlightPlanPage } from './pages/flightplan-page';

const router = createBrowserRouter([
  {
    path: '/',
    element: <AppShell />,
    children: [
      { index: true, element: <FlightPlanPage /> },
      { path: 'charts', element: <ComingSoonPage /> },
      { path: 'settings', element: <ComingSoonPage /> },
    ],
  },
]);

function App() {
  return <RouterProvider router={router} />;
}

export default App;
