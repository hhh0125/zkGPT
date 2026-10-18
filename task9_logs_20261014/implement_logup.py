from pathlib import Path
p=Path('src/range_logup.cpp');s=p.read_text(); start=s.index('LogUpProof proveLogUp('); mid=s.index('bool verifyLogUp(',start)
a=s[start:mid]; b=s[mid:]
# New field-table API reuses the exact reciprocal/sumcheck/IPA machinery;
# legacy functions are retained verbatim to preserve legacy challenge vectors.
cut=a.index('    LogUpProof proof;')
a='''LogUpProof proveFieldLogUp(
    std::size_t query_index, const std::vector<Fr> &value_fields,
    const std::vector<Fr> &table_fields, const std::vector<ll> &counts,
    const std::vector<G1> &generators, const G1 &u, int thread_count,
    const Fr &context, bool adversarial_skip_sum_check) {
    if (!isPowerOfTwo(value_fields.size()) || !isPowerOfTwo(table_fields.size()) || counts.size()!=table_fields.size())
        throw std::invalid_argument("field LogUp dimensions");
    const std::size_t table_size=table_fields.size(), chunk_index=0;
    const unsigned chunk_bits=0;
    std::vector<Fr> multiplicity_fields(table_size);
    for (size_t i=0;i<table_size;i++) multiplicity_fields[i]=Fr(counts[i]);
''' + a[cut:]
a=a.replace('range_logup::commitIntegerValues(\n        integer_values,generators,thread_count)','commitFieldValues(value_fields,generators,thread_count)').replace('range_logup::commitIntegerValues(\n        integer_table,generators,thread_count)','commitPublicTable(table_fields,generators)').replace('integer_multiplicity,generators,thread_count','counts,generators,thread_count')
a=a.replace('Transcript transcript("zkGPT-logup-v1");','Transcript transcript("zkGPT-exp-field-logup-v4");\n    transcript.appendFr("context",context);')
a=a.replace('if (proof.reciprocal_sum!=table_reciprocal_sum)','if (!adversarial_skip_sum_check && proof.reciprocal_sum!=table_reciprocal_sum)')
a=a.replace('hyraxMleOpenProve(\n        values,','hyraxMleOpenProveFr(\n        value_fields,')
a += '\nstd::vector<G1> commitLookupFields(const std::vector<Fr> &v,const std::vector<G1> &g,int threads) { return commitFieldValues(v,g,threads); }\n'
cut0=b.index('        if (proof.query_index');cut1=b.index('        Transcript transcript')
b='''bool verifyFieldLogUp(
    std::size_t query_size, std::size_t query_index,
    const std::vector<Fr> &table_fields, const std::vector<G1> &chunk_commitment,
    const LogUpProof &proof, const std::vector<G1> &generators,
    const G1 &u, const Fr &context, std::string *error) {
    try {
        const std::size_t chunk_index=0, table_size=table_fields.size();
        const unsigned chunk_bits=0;
''' + b[cut0:cut1].replace('query.padded_query_size','query_size').replace('        std::vector<Fr> table_fields(table_size);\n        for (std::size_t i=0;i<table_size;++i) table_fields[i]=Fr(i);\n','') + b[cut1:].replace('query.padded_query_size','query_size')
b=b.replace('Transcript transcript("zkGPT-logup-v1");','Transcript transcript("zkGPT-exp-field-logup-v4");\n        transcript.appendFr("context",context);\n        for (const auto &v:table_fields) if ((offsetPlaceholder+v).isZero()) {}')
b=b.replace('\n        for (const auto &v:table_fields) if ((offsetPlaceholder+v).isZero()) {}','')
b=b.replace('const Fr offset=transcript.challenge("logup.offset");','const Fr offset=transcript.challenge("logup.offset");\n        for (const auto &v:table_fields) if ((offset+v).isZero()) return failWith(error,"zero public table denominator");')
p.write_text(s+'\n'+a+'\n'+b)
p=Path('src/range_logup.hpp');p.write_text(p.read_text()+'''
// Fixed public arbitrary Fr table. Counts are committed before the pole
// challenge; the caller binds query commitments and the context to its statement.
LogUpProof proveFieldLogUp(std::size_t query_index,
    const std::vector<Fr> &values, const std::vector<Fr> &table,
    const std::vector<ll> &counts, const std::vector<G1> &generators,
    const G1 &u, int threads, const Fr &context,
    bool adversarial_skip_sum_check = false);
bool verifyFieldLogUp(std::size_t query_size, std::size_t query_index,
    const std::vector<Fr> &table, const std::vector<G1> &query_commitment,
    const LogUpProof &, const std::vector<G1> &generators, const G1 &u,
    const Fr &context, std::string *error = nullptr);
std::vector<G1> commitLookupFields(const std::vector<Fr> &,
    const std::vector<G1> &, int threads);
''')
