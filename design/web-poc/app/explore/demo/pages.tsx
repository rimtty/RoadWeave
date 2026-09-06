'use client';
import { createContext, useContext, type ReactNode } from 'react';
import { Tabs, TabsList, TabsTrigger } from '@/components/ui/tabs';
export type StudyPage = 'voice' | 'fleet';
export const StudyPageContext = createContext<StudyPage>('voice');
export function StudyPageProvider({
  page,
  children,
}: {
  page: StudyPage;
  children: ReactNode;
}) {
  return (
    <StudyPageContext.Provider value={page}>
      {children}
    </StudyPageContext.Provider>
  );
}
export function useStudyPage() {
  return useContext(StudyPageContext);
}
export function StudyPageTabs({
  value,
  onChange,
  label = '画面を切り替える',
}: {
  value: StudyPage;
  onChange: (page: StudyPage) => void;
  label?: string;
}) {
  return (
    <Tabs
      value={value}
      onValueChange={(value) => onChange(value as StudyPage)}
      className="study-page-tabs"
    >
      <TabsList aria-label={label}>
        <TabsTrigger value="voice">声・距離</TabsTrigger>
        <TabsTrigger value="fleet">車列・GPS</TabsTrigger>
      </TabsList>
    </Tabs>
  );
}
