'use client';
import { createContext, useContext, type ReactNode } from 'react';
import { demoText, demoView, initialDemo, type DemoState } from './model';
export const DemoContext = createContext<{ state: DemoState; id: number }>({
  state: initialDemo(1, true),
  id: 1,
});
export function useStudy() {
  return useContext(DemoContext);
}
export function DemoText({ template }: { template: string }) {
  const { state, id } = useStudy();
  return <>{demoText(state, template, id >= 55)}</>;
}
export function DemoProvider({
  state,
  id,
  children,
}: {
  state: DemoState;
  id: number;
  children: ReactNode;
}) {
  return (
    <DemoContext.Provider value={{ state, id }}>
      {children}
    </DemoContext.Provider>
  );
}
export function useView() {
  const { state, id } = useStudy();
  return demoView(state, id >= 55);
}
