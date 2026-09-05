const linToDb = (value: number): number => 20 * Math.log10(value);

export const formatDb = (value: number): string => {
	if (value <= 0) return "-INF";

	return linToDb(value).toFixed(1);
};
