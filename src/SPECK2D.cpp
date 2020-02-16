#include "SPECK2D.h"
#include <cassert>
#include <cmath>
#include <iostream>


void speck::SPECK2D::assign_coeffs( double* ptr )
{
    m_coeff_buf.reset( ptr );
}

void speck::SPECK2D::assign_mean_dims( double m, long dx, long dy )
{
    m_data_mean = m;
    m_dim_x     = dx;
    m_dim_y     = dy;
}
    
int speck::SPECK2D::speck2d()
{
    assert( m_coeff_buf != nullptr );               // sanity check
    assert( m_dim_x > 0 && m_dim_y > 0 );           // sanity check

    // Let's do some preparation: gather some values
    long num_of_vals = m_dim_x * m_dim_y;
    auto max_coeff   = speck::make_positive( m_coeff_buf.get(), num_of_vals, m_sign_array );
    long max_coefficient_bits = long(std::log2(max_coeff));
    long num_of_part_levels   = m_num_of_part_levels();
    long num_of_xform_levels  = speck::calc_num_of_xform_levels( std::min( m_dim_x, m_dim_y) );

    // Still preparing: lists and sets
    m_LIS.clear();
    m_LIS.resize( num_of_part_levels );
    for( auto& v : m_LIS )  // Avoid frequent memory allocations.
        v.reserve( 8 );
    m_LSP.reserve( 8 );
    SPECKSet2D root( SPECKSetType::TypeS );
    root.part_level = num_of_xform_levels - 1;
    m_calc_set_size( root, 0 );      // Populate other data fields of root.
    m_LIS[ root.part_level ].push_back( root );

    m_I.part_level = num_of_xform_levels - 1;
    m_I.start_x    = root.length_x;
    m_I.start_y    = root.length_y;
    m_I.length_x   = m_dim_x;
    m_I.length_y   = m_dim_y;

    // Get ready for the quantization loop!  L1598 of speck.c
    m_threshold = std::pow( 2.0, double(max_coefficient_bits) );



    return 0;
}
    
    
//
// Private methods
//
void speck::SPECK2D::m_sorting_pass( )
{


}


void speck::SPECK2D::m_process_S( SPECKSet2D& set )
{
    m_output_set_significance( set );   // It also assigns the significance value to the set
    if( set.signif == Significance::Sig || set.signif == Significance::NewlySig )
    {
        if( set.is_pixel() )
        {
            set.signif = Significance::NewlySig;
            m_output_pixel_sign( set );
            m_LSP.push_back( set ); // A copy is saved to m_LSP.
            set.garbage = true;     // This set will be discarded.
        }
    }
    else
    {
        m_code_S( set );
        set.garbage = true;         // This set will be discarded.
    }
}


void speck::SPECK2D::m_code_S( SPECKSet2D& set )
{
    std::vector< SPECKSet2D > subsets;
    m_partition_S( set, subsets );
    for( auto& s : subsets )
    {
        m_LIS[ s.part_level ].push_back( s );
        m_process_S( s );
    }
}


void speck::SPECK2D::m_partition_S( const SPECKSet2D& set, std::vector<SPECKSet2D>& list ) const
{
    // The top-left set will have these bigger dimensions in case that 
    // the current set has odd dimensions.
    const auto bigger_x = set.length_x - (set.length_x / 2);
    const auto bigger_y = set.length_y - (set.length_y / 2);

    SPECKSet2D TL( SPECKSetType::TypeS );   // Top left set
    TL.part_level = set.part_level + 1;
    TL.start_x    = set.start_x;
    TL.start_y    = set.start_y;
    TL.length_x   = bigger_x;
    TL.length_y   = bigger_y;

    SPECKSet2D TR( SPECKSetType::TypeS );   // Top right set
    TR.part_level = set.part_level + 1;
    TR.start_x    = set.start_x    + bigger_x;
    TR.start_y    = set.start_y;
    TR.length_x   = set.length_x   - bigger_x;
    TR.length_y   = bigger_y;

    SPECKSet2D LL( SPECKSetType::TypeS );   // Lower left set
    LL.part_level = set.part_level + 1;
    LL.start_x    = set.start_x;
    LL.start_y    = set.start_y    + bigger_x;
    LL.length_x   = set.length_x;
    LL.length_y   = set.length_y   - bigger_y;

    SPECKSet2D LR( SPECKSetType::TypeS );   // Lower right set
    LR.part_level = set.part_level + 1;
    LR.start_x    = set.start_x    + bigger_x;
    LR.start_y    = set.start_y    + bigger_x;
    LR.length_x   = set.length_x   - bigger_x;
    LR.length_y   = set.length_y   - bigger_y;

    list.clear();     
    list.reserve( 4 );
    list.push_back( LR );   // Put them in the list the same order as in QccPack.
    list.push_back( LL );
    list.push_back( TR );
    list.push_back( TL );
}


// It outputs by printing out the value right now.
void speck::SPECK2D::m_output_set_significance( SPECKSet2D& set ) const
{
    // Sanity check
    assert( set.type == SPECKSetType::TypeS );
    assert( m_significance_map.size() == m_dim_x * m_dim_y );

    set.signif = Significance::Insig;
    for( long y = set.start_y; y < (set.start_y + set.length_y); y++ )
    {
        for( long x = set.start_x; x < (set.start_x + set.length_x); x++ )
        {
            long idx = y * m_dim_x + x;
            if( m_significance_map[ idx ] )
            {
                set.signif = Significance::Sig;
                break;
            }
        }
        if( set.signif == Significance::Sig )
            break;
    }

    if( set.signif == Significance::Sig )
        std::cout << "sorting: set significance = 1" << std::endl;
    else
        std::cout << "sorting: set significance = 0" << std::endl;
    
}


// It outputs by printing out the value right now.
void speck::SPECK2D::m_output_pixel_sign( const SPECKSet2D& pixel )
{
    auto x   = pixel.start_x;
    auto y   = pixel.start_y;
    auto idx = y * m_dim_x * x;
    if( m_sign_array[ idx ] )
        std::cout << "sorting: pixel sign = 1" << std::endl;
    else
        std::cout << "sorting: pixel sign = 0" << std::endl;

    m_coeff_buf[ idx ] -= m_threshold;
}

    
// Calculate the number of partition levels in a plane.
long speck::SPECK2D::m_num_of_part_levels() const
{
    long num_of_lev = 1;    // Even no partition is performed, there's already one level.
    long dim_x = m_dim_x, dim_y = m_dim_y;
    while( dim_x > 1 || dim_y > 1 )
    {
        num_of_lev++;
        dim_x -= dim_x / 2;
        dim_y -= dim_y / 2;
    }
    return num_of_lev;
}



void speck::SPECK2D::m_calc_set_size( SPECKSet2D& set, long subband ) const
{
    assert( subband >= 0 && subband <= 3 );
    long part_level = set.part_level;
    long low_len_x, high_len_x;
    long low_len_y, high_len_y;
    speck::calc_approx_detail_len( m_dim_x, part_level, low_len_x, high_len_x );
    speck::calc_approx_detail_len( m_dim_y, part_level, low_len_y, high_len_y );
    
    // Note: the index of subbands (0, 1, 2, 3) follows what's used in QccPack,
    //       and is different from what is described in Figure 4 of the Pearlman paper.
    if( subband == 0 )      // top left
    {
        set.start_x  = 0;
        set.length_x = low_len_x;
        set.start_y  = 0;
        set.length_y = low_len_y;
    }
    else if( subband == 1 ) // bottom left
    {
        set.start_x  = 0;
        set.length_x = low_len_x;
        set.start_y  = low_len_y;
        set.length_y = high_len_y;
    }
    else if( subband == 2 ) // top right
    {
        set.start_x  = low_len_x;
        set.length_x = high_len_x;
        set.start_y  = 0;
        set.length_y = low_len_y;
    }
    else                    // bottom right
    {
        set.start_x  = low_len_x;
        set.length_x = high_len_x;
        set.start_y  = low_len_y;
        set.length_y = high_len_y;
    }
}


//
// Class SPECKSet2D
//
bool speck::SPECKSet2D::is_pixel() const
{
    return ( length_x == 1 && length_y == 1 );
}

// Constructor
speck::SPECKSet2D::SPECKSet2D( SPECKSetType t )
                 : type( t )
{ }